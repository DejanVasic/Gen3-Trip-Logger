
const button = document.getElementById('resetAvSpeed');
//const textNode = button.childNodes[button.childNodes.length - 1];
const textNode = button.querySelector('.button-text'); 
window.addEventListener('load', () => {
  button.addEventListener('click', resetAvSpeed);
});
var msecReset = 0;
var distReset = 0;
var msec = 0;
var distance = 0;
var speed = 0;
//var prevDistance = 0;
//var prevMsec = 0;
const tripKmElement = document.getElementById('tripKm');
const dteElement = document.getElementById('dte');
const timeHMElement = document.getElementById('timeHM');
const tempElement = document.getElementById('temp');
const numSatElement = document.getElementById('numSat');
const avSpeedElement = document.getElementById('avSpeed');
const avSpeedRElement = document.getElementById('avSpeedR');
const priusRElement = document.getElementById('prius');
const speedElement = document.getElementById('speed');
const roomTempElement = document.getElementById('tempRoomC');
//const debugTextElement = document.getElementById('debugText');

// Create Consumption Gauge
var gaugeAvCons = new LinearGauge({
  animationDuration: 100,
  renderTo: 'gauge-avCons',
  width: 100,
  height: 300,
  units: "l/100km",
  minValue: 0,
  maxValue: 15,
  majorTicks: ["0", "2", "4", "6", "8", "10", "15"],
  minorTicks: 2,
  strokeTicks: true,
  highlights: [
    { from: 0, to: 4, color: 'rgba(144, 232, 206, .6)' },
    { from: 4, to: 7, color: 'rgba(244, 139, 54, .6)' },
    { from: 7, to: 15, color: 'rgba(244, 54, 98, .6)' }
  ],
  colorPlate: "rgba(0, 0, 0, 0)",
  colorUnits: "#90E8CE",
  colorValueText: "#90E8CE",
  colorValueBoxBackground: "rgba(0, 0, 0, 0)",
  colorNumbers: "#90E8CE",
  borderShadowWidth: 0,
  borders: false,
  barBeginCircle: false,
  tickSide: "left",
  numberSide: "left",
  needleSide: "center",
  needleType: "arrow",
  needleWidth: 4,
  barWidth: 10,
  valueBox: true,
  valueTextShadow: false,
  value: 0,
  vertical: true,
  fontNumbers: "Orbitron",
  fontNumbersSize: 30,
  fontValue: "Orbitron",
  fontValueSize: 40,
  valueInt: 1,
  valueDec: 1
}).draw();

// Create Tank Gauge
var gaugeTank = new LinearGauge({
  animationDuration: 100,
  renderTo: 'gauge-tank',
  width: 100,
  height: 300,
  units: "L",
  minValue: 0,
  maxValue: 45,
  majorTicks: ["0", "5", "10", "15", "20", "25", "30", "35", "40", "45"],
  minorTicks: 2,
  strokeTicks: true,
  highlights: [
    { from: 5, to: 10, color: 'rgba(244, 139, 54, .6)' },
    { from: 0, to: 5, color: 'rgba(244, 54, 98, .6)' }
  ],
  colorPlate: "rgba(0, 0, 0, 0)",
  colorUnits: "#90E8CE",
  colorNumbers: "#90E8CE",
  colorValueText: "#90E8CE",
  colorValueBoxBackground: "rgba(0, 0, 0, 0)",
  borderShadowWidth: 0,
  borders: false,
  barBeginCircle: false,
  tickSide: "left",
  numberSide: "left",
  needleSide: "center",
  needleType: "arrow",
  needleWidth: 4,
  barWidth: 10,
  valueBox: true,
  valueTextShadow: false,
  value: 0,
  vertical: true,
  fontNumbers: "Orbitron",
  fontNumbersSize: 30,
  fontValue: "Orbitron",
  fontValueSize: 40,
  valueInt: 1,
  valueDec: 1
}).draw();

// Create Temperature Gauge
var gaugeTemp = new LinearGauge({
  animationDuration: 100,
  renderTo: 'gauge-temperature',
  width: 100,
  height: 300,
  units: "°C",
  minValue: 0,
  maxValue: 120,
  majorTicks: ["0", "20", "40", "60", "80", "100", "120"],
  minorTicks: 2,
  strokeTicks: true,
  highlights: [
    { from: 0, to: 60, color: 'rgba(144, 232, 206, .6)' },
    { from: 60, to: 90, color: 'rgba(244, 139, 54,.6)' },
    { from: 90, to: 120, color: 'rgba(244, 54, 98, .6)' }
  ],
  colorPlate: "rgba(0, 0, 0, 0)",
  colorUnits: "#90E8CE",
  colorNumbers: "#90E8CE",
  colorValueText: "#90E8CE",
  colorValueBoxBackground: "rgba(0, 0, 0, 0)",
  borderShadowWidth: 0,
  borders: false,
  barBeginCircle: false,
  tickSide: "left",
  numberSide: "left",
  needleSide: "center",
  needleType: "arrow",
  needleWidth: 4,
  barWidth: 10,
  valueBox: true,
  valueTextShadow: false,
  value: 0,
  vertical: true,
  fontNumbers: "Orbitron",
  fontNumbersSize: 30,
  fontValue: "Orbitron",
  fontValueSize: 40,
  valueInt: 1,
  valueDec: 1
}).draw();

// Create RPM Gauge
var gaugeRpm = new RadialGauge({
  animationDuration: 100,
  renderTo: 'gauge-rpm',
  width: 300,
  height: 300,
  units: "X 1000",
  minValue: 0,
  maxValue: 6000,
  majorTicks: ["0", "1", "2", "3", "4", "5", "6"],
  minorTicks: 5,
  strokeTicks: true,
  highlights: [
    { from: 0, to: 2000, color: 'rgba(144, 232, 206, .6)' },
    { from: 2000, to: 4000, color: 'rgba(244, 139, 54, .6)' },
    { from: 4000, to: 6000, color: 'rgba(244, 54, 98, .6)' }
  ],
  colorPlate: "rgba(0, 0, 0, 0)",
  colorValueText: "#90E8CE",
  colorValueBoxBackground: "rgba(0, 0, 0, 0)",
  borderShadowWidth: 0,
  borders: false,
  needleType: "arrow",
  needleWidth: 4,
  colorNeedle: "#8d8b8b",
  colorNeedleEnd: "#444444",
  needleCircleSize: 2,
  needleCircleOuter: true,
  needleCircleInner: false,
  valueBox: true,
  valueTextShadow: false,
  value: 0,
  fontNumbers: "Orbitron",
  fontNumbersSize: 30,
  fontUnitsSize: 15,
  fontValue: "Orbitron",
  fontValueSize: 40,
  fontUnits: "Orbitron",
  colorUnits: "#90E8CE",
  colorNumbers: "#90E8CE",
  colorValue: "#90E8CE",
  valueInt: 3,
  valueDec: 0,
}).draw();

var gaugeAngle = new LinearGauge({
  renderTo: 'gauge-angle',
  animationDuration: 100,
  width: 300,
  height: 130,
  minValue: -346,
  maxValue: 346,
  majorTicks: ["<", "|", ">"],
  minorTicks: 2,
  strokeTicks: true,
  colorPlate: "rgba(0, 0, 0, 0)",
  colorNumbers: "#90E8CE",
  highlights: [
    { from: -346, to: -270, color: 'rgba(244, 54, 98, .6)' },
    { from: -270, to: -180, color: 'rgba(244, 139, 54, .6)' },
    { from: -180, to: -90, color: 'rgba(244, 139, 54, .5)' },
    { from: -90, to: 90, color: 'rgba(144, 232, 206, .4)' },
    { from: 90, to: 180, color: 'rgba(244, 139, 54, .5)' },
    { from: 180, to: 270, color: 'rgba(244, 139, 54, .6)' },
    { from: 270, to: 346, color: 'rgba(244, 54, 98, .6)' }
  ],
  borderShadowWidth: 0,
  borders: false,
  barBeginCircle: false,
  tickSide: "left",
  numberSide: "left",
  needleSide: "left",
  needleType: "line",
  needleWidth: 4,
  barWidth: 0,
  valueBox: false,
  value: 0,
  fontUnits: "Orbitron",
  fontNumbers: "Orbitron",
  fontNumbersSize: 30,
}).draw();

function resetAvSpeed() {
  msecReset = msec;
  distReset = distance;
  //speedElement.textContent = msec;
}

if (!!window.EventSource) {
  var source = new EventSource('/events');
  source.addEventListener('new_readings', function (e) {
    //var jsonObj = JSON.parse(e.data);
    var jsonObj = JSON.parse(e.data);
    //console.log(jsonObj);
    gaugeTank.value = jsonObj.tank;
    gaugeTemp.value = jsonObj.temp;
    gaugeRpm.value = jsonObj.rpm;
    distance = jsonObj.dist;
    var consLpH = jsonObj.cons;
    var angle = jsonObj.sAng;
    msec = jsonObj.msec;
    //textNode.nodeValue = '#' + jsonObj.tripCounter;
    textNode.textContent = '#' + jsonObj.tripCounter;
    gaugeAvCons.value = distance === 0 ? consLpH : 100 * consLpH / distance;
    timeHMElement.textContent = msToTime(msec);
    tripKmElement.textContent = distance.toFixed(1);
    avSpeedElement.textContent = (3600000 * distance / msec).toFixed();
    var tempSpeed = 0;
    if (distReset !== 0) {
      tempSpeed = 3600000 * (distance - distReset) / (msec - msecReset);
      avSpeedRElement.innerHTML = '<br>' + tempSpeed.toFixed();
      updatePriusColor(tempSpeed);
    }
    //tempSpeed = distance === 0 ? 0 : (distance - prevDistance) / (msec - prevMsec) * 3600000
    speed = jsonObj.speed;
    speedElement.textContent = speed.toFixed();
    speed > 131 ? speedElement.style.color = 'rgb(244, 54, 98)' : speed > 55 ? speedElement.style.color = 'rgb(244, 139, 54)' : speedElement.style.color = 'rgb(144, 232, 206)';
    roomTempElement.textContent = (jsonObj.tempRoomC).toFixed();
    numSatElement.textContent = (jsonObj.numSat + 1).toFixed();
    dteElement.textContent = (gaugeTank.value * (distance + 100) / (consLpH + 5.5)).toFixed(); //5.5 = ~real anual consumption
    if (angle > 1000) angle = angle - 4095;
    gaugeAngle.value = angle * -1;
    debugTextElement.textContent = e.data;
  }, false);
}

function updatePriusColor(avSpeed) {
  if (avSpeed > 130) {
    priusRElement.style.filter = "invert(0.35) sepia(1) saturate(10) hue-rotate(320deg)";
  } else {
    priusRElement.style.filter = "invert(0.15) sepia(1) saturate(4) hue-rotate(130deg)";
  }
}

function msToTime(milliseconds) {
  //Get hours from milliseconds
  var hours = milliseconds / (1000 * 60 * 60);
  var absoluteHours = Math.floor(hours);
  var h = absoluteHours > 9 ? absoluteHours : '0' + absoluteHours;

  //Get remainder from hours and convert to minutes
  var minutes = (hours - absoluteHours) * 60;
  var absoluteMinutes = Math.floor(minutes);
  var m = absoluteMinutes > 9 ? absoluteMinutes : '0' + absoluteMinutes;

  //Get remainder from minutes and convert to seconds
  var seconds = (minutes - absoluteMinutes) * 60;
  var absoluteSeconds = Math.floor(seconds);
  var s = absoluteSeconds > 9 ? absoluteSeconds : '0' + absoluteSeconds;

  return h == "00" ? m + ':' + s : h + ':' + m + ':' + s;
}