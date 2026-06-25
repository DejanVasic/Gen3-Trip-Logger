const PriusESP32 = SpreadsheetApp.openById("1MY7rprDmPVs06Jik2sF1Y-LCJc9weHHboOkqMk8uoIU")
const PriusSh = PriusESP32.getSheetByName("Prius")

// =============================================================================
// Helper: parses a GPS DateTime field into milliseconds since epoch (UTC).
// The value may be a Date object (as Sheets returns it), a string, or a number.
// Returns null if it cannot be parsed.
//
// Locale-safe: uses explicit regexes for known formats, does not rely on the
// JS Date parser (which is locale-dependent in some implementations).
// Supported formats:
//   - Date object (as Sheets returns when it parses successfully)
//   - "DD.MM.YYYY HH:MM:SS" or "DD.MM.YYYY. HH:MM:SS" (sr/EU)
//   - "MM/DD/YYYY HH:MM:SS" (US)
//   - "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS" (ISO)
// =============================================================================
function parseGpsDateTime(val) {
  if (val == null || val === "" || val === 0) return null
  
  // Case 1: Date object (Sheets parsed it successfully).
  // IMPORTANT: Sheets stores datetime as a "naive" value in the sheet timezone.
  // val.getTime() would return milliseconds interpreting that value as LOCAL time.
  // Since the GPS DateTime column CONTAINS a UTC value (as its header states),
  // we extract the LOCAL components and construct a UTC epoch from them.
  // Example: Sheets returns a Date with local components 2026-06-25 05:22:56,
  // which is actually UTC time - so Date.UTC(2026, 5, 25, 5, 22, 56) is the correct epoch.
  if (val instanceof Date) {
    const t = Date.UTC(
      val.getFullYear(), val.getMonth(), val.getDate(),
      val.getHours(), val.getMinutes(), val.getSeconds()
    )
    if (t > 1577836800000) return t  // > 2020-01-01
    return null
  }
  
  // Case 2: string - try several known formats
  if (typeof val === "string") {
    const s = val.trim()
    let m
    
    // Format: DD.MM.YYYY HH:MM:SS (with or without trailing dot after year)
    m = s.match(/^(\d{1,2})\.(\d{1,2})\.(\d{4})\.?\s+(\d{1,2}):(\d{2}):(\d{2})$/)
    if (m) {
      const t = Date.UTC(+m[3], +m[2] - 1, +m[1], +m[4], +m[5], +m[6])
      if (t > 1577836800000) return t
    }
    
    // Format: MM/DD/YYYY HH:MM:SS (US)
    m = s.match(/^(\d{1,2})\/(\d{1,2})\/(\d{4})\s+(\d{1,2}):(\d{2}):(\d{2})$/)
    if (m) {
      const t = Date.UTC(+m[3], +m[1] - 1, +m[2], +m[4], +m[5], +m[6])
      if (t > 1577836800000) return t
    }
    
    // Format: YYYY-MM-DD HH:MM:SS or YYYY-MM-DDTHH:MM:SS (ISO)
    m = s.match(/^(\d{4})-(\d{1,2})-(\d{1,2})[T\s](\d{1,2}):(\d{2}):(\d{2})/)
    if (m) {
      const t = Date.UTC(+m[1], +m[2] - 1, +m[3], +m[4], +m[5], +m[6])
      if (t > 1577836800000) return t
    }
  }
  
  // Case 3: number (Sheets serial date - days since 1899-12-30)
  if (typeof val === "number" && val > 30000 && val < 100000) {
    // Convert Sheets serial -> milliseconds.
    // Sheets epoch is 1899-12-30 (Excel-compatible).
    const sheetsEpochMs = Date.UTC(1899, 11, 30)
    const t = sheetsEpochMs + val * 86400000
    if (t > 1577836800000) return t
  }
  
  return null
}

// =============================================================================
// Helper: if the Unix Date in a row is wrong due to an ESP32 NTP issue
// (wake-up row with a stale RTC value), returns the corrected value in seconds
// since epoch. Returns null if no fix is needed or possible.
//
// Logic - three scenarios:
//   1. Not a wake-up row (millis >= 15000) => returns null (leave alone)
//   2. Wake-up with a valid GPS DateTime:
//      - if Unix vs GPS differs by > 30s, use GPS as the source of truth
//   3. Wake-up without GPS (referenceUnix and referenceMillis provided):
//      - computes expected Unix from reference: refUnix - (refMillis - millis)/1000
//      - if that differs from the current Unix by more than 30s, fixes it
//      - the "reference" is another row in the same trip that is NOT a wake-up
//
// Parameters:
//   unixSec         - current Unix value (number, seconds since epoch)
//   millis          - Milliseconds field from that row
//   gpsDt           - GPS DateTime field from that row (Date | string | 0 | null)
//   referenceUnix   - optional: Unix from a non-wakeup row in the same trip
//   referenceMillis - optional: Millisec from the same reference row
// =============================================================================
function maybeFixUnixDate(unixSec, millis, gpsDt, referenceUnix, referenceMillis) {
  if (millis == null || millis >= 15000) return null  // not a wake-up
  if (unixSec == null || unixSec < 1700000000) return null  // unix already invalid, leave alone
  
  // Scenario 1: GPS available - best source of truth
  const gpsMs = parseGpsDateTime(gpsDt)
  if (gpsMs != null) {
    const gpsSec = Math.round(gpsMs / 1000)
    const diff = Math.abs(unixSec - gpsSec)
    if (diff <= 30) return null
    return gpsSec
  }
  
  // Scenario 2: no GPS - try the fallback via a reference row in the same trip
  if (referenceUnix != null && referenceUnix > 1700000000 && 
      referenceMillis != null && referenceMillis >= 15000) {
    // The wake-up row is EARLIER than the reference (smaller millis = earlier in the trip).
    // Assumption: the reference is a non-wakeup row so it has a correct unix.
    // Expected unix = ref_unix - (ref_millis - millis) / 1000
    const expectedUnix = referenceUnix - Math.round((referenceMillis - millis) / 1000)
    const diff = Math.abs(unixSec - expectedUnix)
    if (diff <= 30) return null  // unix already OK
    return expectedUnix
  }
  
  return null  // nothing to compare against
}


// =============================================================================
// Helper: in a sorted Tabela, find the nearest non-wakeup row in the same trip
// to use as a reference for the fallback fix. Tabela must be sorted by
// (TRIP # desc, millis desc) - as sortFunction produces.
//
// Returns [refUnix, refMillis] or [null, null] if no reference is found.
//
// Parameters:
//   Tabela    - 2D array from Sorter (sorted)
//   i         - index of the row being fixed
//   tripCol   - index of the TRIP # column in Tabela (8 in Sorter)
//   unixCol   - index of the Unix column in Tabela (0)
//   millisCol - index of the Millisec column in Tabela (1)
// =============================================================================
function findReferenceRow(Tabela, i, tripCol, unixCol, millisCol) {
  const trip = Tabela[i][tripCol]
  // In a Tabela sorted by (trip desc, millis desc):
  //   - a row with larger millis in the same trip is ABOVE (smaller i)
  //   - a row with smaller millis in the same trip is BELOW (larger i)
  // A wake-up row has SMALL millis so it's usually at the BOTTOM of its trip block.
  // We look for a non-wakeup row - those have larger millis so they're ABOVE.
  
  // Search BACKWARD (smaller indices = larger millis in the same trip)
  for (let j = i - 1; j >= 0; j--) {
    if (Tabela[j][tripCol] != trip) break  // left the trip block
    if (Tabela[j][millisCol] >= 15000 && Tabela[j][unixCol] > 1700000000) {
      return [Tabela[j][unixCol], Tabela[j][millisCol]]
    }
  }
  // Search FORWARD (larger indices = smaller millis in the same trip)
  // Less likely for a wake-up row, but possible if there is an even smaller wake-up below.
  for (let j = i + 1; j < Tabela.length; j++) {
    if (Tabela[j][tripCol] != trip) break
    if (Tabela[j][millisCol] >= 15000 && Tabela[j][unixCol] > 1700000000) {
      return [Tabela[j][unixCol], Tabela[j][millisCol]]
    }
  }
  return [null, null]
}

function Mapiranje(e) {
  var Shit = e.source.getActiveSheet()

  if (Shit.getName() == "Map") {
    var Red = e.range.getRow()
    var Kolona = e.range.getColumn()

    if (Red == 1 && Kolona == 1) { // A1
      var Vrednost = e.range.getValue()
      var Start = true
      var lastRow = PriusSh.getLastRow()
      var Tabela = PriusSh.getRange(1, 2, lastRow, 14).getValues()
      var Opis = [9]
      var AltStart = 0
      var pLat = 0
      var plon = 0
      // Create a map
      var map = Maps.newStaticMap()
      var parking = Maps.newStaticMap()


      // Remove all map images
      Shit.getImages().forEach(function (i) { i.remove() })

      for (var i = lastRow - 1; i > 0; i--) {
        if (Tabela[i][7] == Vrednost && Tabela[i][4] != 0) {
          if (Start) {
            map.setMarkerStyle(Maps.StaticMap.MarkerSize.SMALL, "0x00FF00", "Green")
            pLat = Tabela[i][4]
            plon = Tabela[i][5]
            Start = false

            Opis[0] = [Tabela[i][7]]
            Opis[1] = [Tabela[i][12]]
            Opis[2] = [Tabela[i][8]]
            Opis[3] = [0]
            Opis[4] = [0]
            Opis[5] = [Tabela[i][10]]
            Opis[6] = [Tabela[i][11]]
            Opis[7] = [0]
            Opis[8] = [0]


            AltStart = Tabela[i][6]
          } else if (Tabela[i - 1][7] != Vrednost) {
            Opis[0] = [Tabela[i][7]]
            Opis[1] = [Tabela[i][12]]
            Opis[2] = [Tabela[i][8]]
            Opis[3] = [100 * Tabela[i][9] / Tabela[i][8]]
            Opis[4] = [3600000 * Tabela[i][8] / Tabela[i][0]]
            Opis[5] = [Tabela[i][10]]
            Opis[6] = [Tabela[i][11]]
            Opis[7] = [100 / Opis[2] * Opis[5]]
            Opis[8] = [Tabela[i][6] - AltStart]
            pLat = Tabela[i][4]
            plon = Tabela[i][5]
            map.setMarkerStyle(Maps.StaticMap.MarkerSize.SMALL, "0xFF0000", "Red")
            map.addMarker(Tabela[i][4], Tabela[i][5])
            break
          } else {
            map.setMarkerStyle(Maps.StaticMap.MarkerSize.SMALL, "0x0000FF", "Blue")
          }
          map.addMarker(Tabela[i][4], Tabela[i][5])
        } else if (!Start) {
          break
        }
      }
      parking.setMarkerStyle(Maps.StaticMap.MarkerSize.SMALL, "0xFF8000", "Orange")
      parking.addMarker(pLat, plon)
      Shit.insertImage(Utilities.newBlob(parking.getMapImage(), "image/png", 1), 4, 1)
      Shit.insertImage(Utilities.newBlob(map.getMapImage(), "image/png", 1), 4, 26)
      e.range.setValue("TRIP #")
      Logger.log(Opis.toString())
      Shit.getRange(1, 2, 9).setValues(Opis)

    }
  }
}



function InsertTriggers() {
  // Get all existing triggers for this project.
  const allTriggers = ScriptApp.getProjectTriggers();

  // Check for the 'Sorter' trigger.
  let sorterTriggerExists = false;
  for (const trigger of allTriggers) {
    if (trigger.getHandlerFunction() === 'Sorter') {
      sorterTriggerExists = true;
      break;
    }
  }

  // Create the 'Sorter' trigger if it doesn't exist.
  if (!sorterTriggerExists) {
    ScriptApp.newTrigger('Sorter')
      .timeBased()
      .everyMinutes(5)
      .create();
    console.log('"Sorter" trigger created.');
  } else {
    console.log('"Sorter" trigger already exists.');
  }

  // Check for the 'Mapiranje' trigger.
  let mapiranjeTriggerExists = false;
  for (const trigger of allTriggers) {
    if (trigger.getHandlerFunction() === 'Mapiranje') {
      mapiranjeTriggerExists = true;
      break;
    }
  }

  // Create the 'Mapiranje' trigger if it doesn't exist.
  if (!mapiranjeTriggerExists) {
    ScriptApp.newTrigger('Mapiranje')
      .forSpreadsheet(PriusESP32)
      .onEdit()
      .create();
    console.log('"Mapiranje" trigger created.');
  } else {
    console.log('"Mapiranje" trigger already exists.');
  }
}



function Sorter() {
  var lastRow = PriusSh.getLastRow() - 1
  //PriusSh.getRange("M:M").setNumberFormat("0")
  var Tabela = PriusSh.getRange(2, 1, lastRow, 18).getValues()

  if (Tabela[lastRow - 1][14] != "") { return } //no new data appended

  Tabela.sort(sortFunction)
  var LastTrip = "Tank trip: "
  var Put = 0
  var Litara = 0
  var LastRefill = 0
  var TankRefill = 0
  //var SheetsEpochMillis = new Date(1899, 11, 30).getTime()
  //  A   B   C   D   E   F   G   H   I   J   K   L   M   N   O   P   Q
  //  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16

  for (var i = 0; i < lastRow; i++) {

    if (Tabela[i][14] == "") { // If column O is empty = new row

      // === FIX UNIX DATE FOR WAKE-UP ROWS ===
      // ESP32 sometimes writes a stale RTC value into the Unix Date field when
      // it wakes from deep sleep, before NTP has had time to synchronize.
      // Detected by Milliseconds < 15000 and a > 30s mismatch against GPS DateTime.
      // If GPS is missing, uses a non-wakeup row from the same trip as a reference.
      var refData = findReferenceRow(Tabela, i, 8, 0, 1)
      var fixedUnix = maybeFixUnixDate(Tabela[i][0], Tabela[i][1], Tabela[i][4],
                                       refData[0], refData[1])
      if (fixedUnix != null) {
        Tabela[i][0] = fixedUnix
      }
      // === END FIX ===

      Tabela[i][13] = Tabela[i][1] > 0 ? Tabela[i][1] / 86400000 : "" //1000 * 60 * 60 * 24 

      if (Tabela[i][12] instanceof Date) { //appended row picked hh:mm:ss formatting and than GS converted its milliseconds to time
        var Kolko = Tabela[i][12].getTime()
        if (Kolko > -2.2090788E12) {  //> Date(1899, 12, 31).getTime() - cell !contains good duration
          Tabela[i][12] = ((Kolko + 2.2091652E12) / 7.46496E15)  //Kolko - Date(1899, 11, 30).getTime() /  (1000 * 60 * 60 * 24) ** 2
        }
      } else if (Tabela[i][12] > 1) { //convert millis to duration 
        Tabela[i][12] = Tabela[i][12] / 86400000 //1000 * 60 * 60 * 24 
      }

      if (Tabela[i][0] > 1E9) {
        Tabela[i][14] = Utilities.formatDate(new Date(Tabela[i][0] * 1000), PriusESP32.getSpreadsheetTimeZone(), "dd.MM.yyyy. HH:mm:ss")
      } else {
        Tabela[i][14] = 0
      }

      Tabela[i][15] = (Tabela[i][9] > 0 && Tabela[i][1] > 0) ? 3600000 * Tabela[i][9] / Tabela[i][1] : ""

      Tabela[i][16] = Tabela[i][9] > 0 ? 100 * Tabela[i][10] / Tabela[i][9] : 0

      if (i + 1 < lastRow) {

        if (Tabela[i][3] == 0) { //car was in Ignition-On Mode (after Accessory Mode, without brake pedal pressed) => copy previous known value
          var tempTank = 0
          for (var t = i + 1; t < lastRow; t++) {
            if (Tabela[t][3] > 0) {
              tempTank = Tabela[t][3]
              break
            }
          }
          Tabela[i][3] = tempTank
        } else if (Tabela[i][3] - 5 > Tabela[i + 1][3] && Tabela[i][8] != Tabela[i + 1][8]) {//tank refill?
          TankRefill = i
          for (var a = i + 1; a < lastRow; a++) {
            if (Tabela[a][17].toString().startsWith("Av:")) { //previous refill
              Put = Tabela[i][2] - Tabela[a][2]
              Litara = Tabela[i][3] - Tabela[i + 1][3]
              const CeneSh = PriusESP32.getSheetByName("Gorivo")
              var cena = CeneSh.getRange(7, 3).getValue()
              const Cenabroj = parseFloat(
                cena.replace('RSD', '')   // ukloni valutu
                  .trim()               // skloni razmake
                  .replace(',', '.')    // zarez → tačka
              );
              Tabela[i][17] = "Av:" + (100 * Litara / Put).toFixed(2) + " l/100km (" + Litara.toFixed(1) + "l, " + Put.toFixed(0) + "km) " + " Refill: " + Cenabroj.toFixed(2) + " x " + (Tabela[i][3] - Tabela[i + 1][3]).toFixed(2) + " = " + (Cenabroj.toFixed(2) * (Tabela[i][3] - Tabela[i + 1][3])).toFixed(2)
              break
            }
          }
        }
      }
    }

    if (LastRefill == 0 && Tabela[i][17].toString().startsWith("Av:") && i > 0) {
      LastRefill = i
    }
  }

  Put = Tabela[0][2] - Tabela[LastRefill][2]
  Litara = Tabela[LastRefill][3] - Tabela[0][3]

  //="Tank trip: " & (C2 - (INDEX(FILTER(C2:C; REGEXMATCH(P2:P; "^Av:") ); 1))) & " km   " & text(100 * (INDEX(FILTER(D2:D; REGEXMATCH(P2:P; "^Av:")); 1) - D2) / (C2 - (INDEX(FILTER(C2:C; REGEXMATCH(P2:P; "^Av:") ); 1)));"#0.0") & " l/100km "

  LastTrip += Put.toString() + " km  "
  LastTrip += (100 * Litara / Put).toFixed(1) + " l/100km, OBD: "

  Litara = Tabela[0][10]
  Put = Tabela[0][9]
  for (i = 1; i < LastRefill; i++) {
    if (Tabela[i][8] != Tabela[i - 1][8]) {  //7.7
      Litara += Tabela[i][10]
      Put += Tabela[i][9]
    }
  }

  LastTrip += Number(Put).toFixed(1) + " km "
  LastTrip += (100 * Litara / Number(Put)).toFixed(2) + " l/100km  "

  if (TankRefill) {
    Tabela[TankRefill][17] += "\n" + PriusSh.getRange(1, 18).getValue()
  }

  PriusSh.getRange(1, 18).setValue(LastTrip)
  PriusSh.getRange(2, 1, lastRow, 18).setValues(Tabela)

  PriusSh.getRange("A:C").setNumberFormat("0")
  PriusSh.getRange("D:D").setNumberFormat("0.00")
  PriusSh.getRange("E:E").setNumberFormat("dd.MM.yyyy. HH:mm:ss")
  PriusSh.getRange("F:G").setNumberFormat("0.00000000")
  PriusSh.getRange("H:I").setNumberFormat("0")
  PriusSh.getRange("J:L").setNumberFormat("0.0")
  PriusSh.getRange("M:N").setNumberFormat("[h]:mm:ss")
  PriusSh.getRange("O:O").setNumberFormat("dd.MM.yyyy. HH:mm:ss")
  PriusSh.getRange("P:Q").setNumberFormat("0.0")
}


function sortFunction(a, b) {
  if (a[8] < b[8]) { return 1 }
  if (a[8] > b[8]) { return -1 }

  if (a[1] < b[1]) { return 1 }
  if (a[1] > b[1]) { return -1 }

  return 0
}