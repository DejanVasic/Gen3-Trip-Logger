const PriusESP32 = SpreadsheetApp.openById("read_from_sharred_google_spreadsheet_url")
const PriusSh = PriusESP32.getSheetByName("Prius")


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
        } else if (Tabela[i][3] - 5 > Tabela[i + 1][3] && Tabela[i][8] != Tabela[i + 1][8] ) {//tank refill?
          TankRefill = i
          for (var a = i + 1; a < lastRow; a++) {
            if (Tabela[a][17].toString().startsWith("Av:")) { //previous refill
              Put = Tabela[i][2] - Tabela[a][2]
              Litara = Tabela[i][3] - Tabela[i + 1][3]
              const CeneSh = PriusESP32.getSheetByName("Gorivo")
              var cena = CeneSh.getRange(7, 3).getValue()
              if (isNaN(cena) && cena.length > 3) { cena = cena.substring(0, 3) }
              Tabela[i][17] = "Av:" + (100 * Litara / Put).toFixed(2) + " l/100km (" + Litara.toFixed(1) + "l, " + Put.toFixed(0) + "km) " + " Refill: " + cena.toFixed(2) + " x " + (Tabela[i][3] - Tabela[i + 1][3]).toFixed(2) + " = " + (cena.toFixed(2) * (Tabela[i][3] - Tabela[i + 1][3])).toFixed(2)
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
  LastTrip += (100 * Litara / Number(Put)).toFixed(1) + " l/100km  "

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
