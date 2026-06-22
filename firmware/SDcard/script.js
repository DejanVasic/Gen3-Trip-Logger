let container = null
let segmentElements = []
const button = document.getElementById('resetAvSpeed')
const textNode = button.querySelector('.button-text')

var msecReset = 0
var distReset = 0
var msec = 0
var distance = 0
var speed = 0
var ccSpeed = 0

const tripKmElement = document.getElementById('tripKm')
const dteElement = document.getElementById('dte')
const timeHMElement = document.getElementById('timeHM')
const numSatElement = document.getElementById('numSat')
const avSpeedElement = document.getElementById('avSpeed')
const avSpeedRElement = document.getElementById('avSpeedR')
const priusRElement = document.getElementById('prius')
const speedElement = document.getElementById('speed')
const ccSpeedElement = document.getElementById('ccSpeed')
const roomTempElement = document.getElementById('tempRoomC')
const totalVoltageElement = document.getElementById('totalVoltage')
const auxVoltageElement = document.getElementById('auxVoltage')
const packImbalanceElement = document.getElementById('packImbalance')


const _cache = {}

function isDirty(key, value, precision) {
    if (precision === undefined) precision = 0.5
    const prev = _cache[key]
    if (prev === undefined) {
        _cache[key] = value
        return true
    }
    if (Math.abs(value - prev) >= precision) {
        _cache[key] = value
        return true
    }
    return false
}

function isDirtyStr(key, str) {
    if (_cache[key] === str) return false
    _cache[key] = str
    return true
}

function setText(el, cacheKey, text) {
    if (!el) return
    if (_cache[cacheKey] === text) return
    _cache[cacheKey] = text
    el.textContent = text
}
function setHtml(el, cacheKey, html) {
    if (!el) return
    if (_cache[cacheKey] === html) return
    _cache[cacheKey] = html
    el.innerHTML = html
}
function setStyle(el, cacheKey, prop, value) {
    if (!el) return
    if (_cache[cacheKey] === value) return
    _cache[cacheKey] = value
    el.style[prop] = value
}


const ALERTS = {
    rpm: { high: 4000 },
    tank: { low: 5 },
    invTemp: { high: 77 },
    temp: { high: 94 },
    avCons: { high: 10 }
}

const RPM_CFG = {
    cx: 150, cy: 150,
    radius: 112,
    startAngle: 225,
    endAngle: 495,
    min: 0,
    max: 6000,
    majorTicks: [0, 1000, 2000, 3000, 4000, 5000, 6000],
    alertStart: 4000
}

function polarToCartesian(cx, cy, r, angleDeg) {
    const a = (angleDeg - 90) * Math.PI / 180.0
    return { x: cx + r * Math.cos(a), y: cy + r * Math.sin(a) }
}

function describeArc(cx, cy, r, startAngle, endAngle) {
    const start = polarToCartesian(cx, cy, r, startAngle)
    const end = polarToCartesian(cx, cy, r, endAngle)
    const largeArc = (endAngle - startAngle) <= 180 ? 0 : 1
    return [
        'M', start.x, start.y,
        'A', r, r, 0, largeArc, 1, end.x, end.y
    ].join(' ')
}

const CLOAD_CFG = {
    innerRadius: 82,
    min: 0,
    max: 100
}

function buildRpmGaugeDual() {
    const svg = document.getElementById('rpm-gauge')
    if (!svg) return

    while (svg.firstChild) svg.removeChild(svg.firstChild)

    const ns = 'http://www.w3.org/2000/svg'
    const { cx, cy, radius, startAngle, endAngle, majorTicks, min, max } = RPM_CFG
    const innerR = CLOAD_CFG.innerRadius

    const bgPath = document.createElementNS(ns, 'path')
    bgPath.setAttribute('class', 'rpm-arc-bg')
    bgPath.setAttribute('d', describeArc(cx, cy, radius, startAngle, endAngle))
    svg.appendChild(bgPath)

    const fillPath = document.createElementNS(ns, 'path')
    fillPath.setAttribute('id', 'rpm-arc-fill')
    fillPath.setAttribute('class', 'rpm-arc-fill')
    fillPath.setAttribute('stroke', '#91E8CE')
    fillPath.setAttribute('d', describeArc(cx, cy, radius, startAngle, endAngle))
    svg.appendChild(fillPath)
    const totalLen = fillPath.getTotalLength()
    fillPath.setAttribute('stroke-dasharray', totalLen)
    fillPath.setAttribute('stroke-dashoffset', totalLen)
    fillPath.dataset.totalLen = totalLen

    const alertR = radius + 13
    const a2000 = startAngle + (2000 - min) / (max - min) * (endAngle - startAngle)
    const a4000 = startAngle + (4000 - min) / (max - min) * (endAngle - startAngle)
    const alertSegments = [
        { from: startAngle, to: a2000, color: '#91E8CE' },
        { from: a2000, to: a4000, color: '#F48B36' },
        { from: a4000, to: endAngle, color: '#F43662' }
    ]
    alertSegments.forEach(s => {
        const p = document.createElementNS(ns, 'path')
        p.setAttribute('class', 'rpm-arc-alert')
        p.setAttribute('stroke', s.color)
        p.setAttribute('d', describeArc(cx, cy, alertR, s.from, s.to))
        svg.appendChild(p)
    })
    const innerBg = document.createElementNS(ns, 'path')
    innerBg.setAttribute('class', 'rpm-arc-inner-bg')
    innerBg.setAttribute('d', describeArc(cx, cy, innerR, startAngle, endAngle))
    svg.appendChild(innerBg)

    const innerFill = document.createElementNS(ns, 'path')
    innerFill.setAttribute('id', 'rpm-arc-inner-fill')
    innerFill.setAttribute('class', 'rpm-arc-inner-fill')
    innerFill.setAttribute('stroke', 'rgba(145, 232, 206, 0.8)')
    innerFill.setAttribute('d', describeArc(cx, cy, innerR, startAngle, endAngle))
    svg.appendChild(innerFill)
    const innerLen = innerFill.getTotalLength()
    innerFill.setAttribute('stroke-dasharray', innerLen)
    innerFill.setAttribute('stroke-dashoffset', innerLen)
    innerFill.dataset.totalLen = innerLen

    const tickOuterR = radius + 10
    const tickInnerR = radius + 2
    const labelR = radius + 22

    majorTicks.forEach((v, i) => {
        const frac = (v - min) / (max - min)
        const ang = startAngle + frac * (endAngle - startAngle)

        const p1 = polarToCartesian(cx, cy, tickInnerR, ang)
        const p2 = polarToCartesian(cx, cy, tickOuterR, ang)
        const line = document.createElementNS(ns, 'line')
        line.setAttribute('x1', p1.x); line.setAttribute('y1', p1.y)
        line.setAttribute('x2', p2.x); line.setAttribute('y2', p2.y)
        line.setAttribute('class', 'rpm-tick rpm-tick-major')
        svg.appendChild(line)

        const lp = polarToCartesian(cx, cy, labelR, ang)
        const txt = document.createElementNS(ns, 'text')
        txt.setAttribute('x', lp.x); txt.setAttribute('y', lp.y)
        txt.setAttribute('class', 'rpm-tick-label')
        txt.textContent = v === 0 ? '0' : (v / 1000) //+ 'k'
        svg.appendChild(txt)
    })

    // minor ticks
    const minorCount = 5
    for (let i = 0; i < majorTicks.length - 1; i++) {
        const v0 = majorTicks[i]
        const v1 = majorTicks[i + 1]
        for (let j = 1; j < minorCount; j++) {
            const v = v0 + (v1 - v0) * (j / minorCount)
            const frac = (v - min) / (max - min)
            const ang = startAngle + frac * (endAngle - startAngle)
            const p1 = polarToCartesian(cx, cy, radius + 4, ang)
            const p2 = polarToCartesian(cx, cy, radius + 10, ang)
            const line = document.createElementNS(ns, 'line')
            line.setAttribute('x1', p1.x); line.setAttribute('y1', p1.y)
            line.setAttribute('x2', p2.x); line.setAttribute('y2', p2.y)
            line.setAttribute('class', 'rpm-tick')
            svg.appendChild(line)
        }
    }
}

function updateRpmGaugeDual(rpmValue, cLoadValue) {
    if (isDirty('rpm', rpmValue, 5)) {
        const fillPath = document.getElementById('rpm-arc-fill')
        const rpmEl = document.getElementById('val-rpm')
        if (fillPath && rpmEl) {
            const { min, max, alertStart } = RPM_CFG
            let v = Math.max(min, Math.min(max, rpmValue))
            const frac = (v - min) / (max - min)
            const totalLen = parseFloat(fillPath.dataset.totalLen)
            fillPath.setAttribute('stroke-dashoffset', totalLen * (1 - frac))

            let color = 'rgba(145, 232, 206, 0.8)'
            if (v >= alertStart) color = '#F43662'
            else if (v >= 2000) color = '#F48B36'
            fillPath.setAttribute('stroke', color)

            rpmEl.textContent = Math.round(v)
            if (v >= alertStart) rpmEl.classList.add('alert-text')
            else rpmEl.classList.remove('alert-text')
        }
    }

    if (isDirty('cLoad', cLoadValue, 0.5)) {
        const innerFill = document.getElementById('rpm-arc-inner-fill')
        const clEl = document.getElementById('val-rpm-cload')
        if (innerFill) {
            const { min, max } = CLOAD_CFG
            let v = Math.max(min, Math.min(max, cLoadValue))
            const frac = (v - min) / (max - min)
            const totalLen = parseFloat(innerFill.dataset.totalLen)
            innerFill.setAttribute('stroke-dashoffset', totalLen * (1 - frac))

            if (clEl) {
                clEl.textContent = Math.round(v) //+ '%'

            }
        }
    }
}

function createScale(id, max, valuesArray) {
    const scaleContainer = document.getElementById('scale-' + id)
    if (!scaleContainer) return

    scaleContainer.innerHTML = ''

    valuesArray.forEach(val => {
        const mark = document.createElement('div')
        mark.className = 'scale-mark'

        const text = document.createElement('span')
        text.textContent = val
        mark.appendChild(text)

        const percent = (val / max) * 100
        mark.style.bottom = percent + '%'

        scaleContainer.appendChild(mark)
    })
}

function updateSolidBar(id, value, max) {
    const isInt = (id === 'rpm' || id === 'cLoad' || id === 'invTemp' || id === 'temp')
    const precision = isInt ? 0.5 : 0.05
    if (!isDirty('bar-' + id, value, precision)) return

    const bar = document.getElementById('bar-' + id)
    const valText = document.getElementById('val-' + id)
    if (!bar || !valText) return

    let percent = (value / max) * 100
    percent = Math.min(100, Math.max(0, percent))
    bar.style.height = percent + '%'

    if (isInt) {
        valText.textContent = Math.round(value)
    } else {
        valText.textContent = Number(value).toFixed(1)
    }

    // Alert 
    const limits = ALERTS[id]
    let isAlert = false
    if (limits) {
        if (limits.high !== undefined && value >= limits.high) isAlert = true
        if (limits.low !== undefined && value <= limits.low) isAlert = true
    }

    if (isAlert) {
        bar.classList.add('alert-bar')
        valText.classList.add('alert-text')
    } else {
        bar.classList.remove('alert-bar')
        valText.classList.remove('alert-text')
    }
}


function updateCurrentBar(value, maxCharge, maxDischarge) {
    if (!isDirty('bar-current', value, 0.1)) return

    const bar = document.getElementById('bar-current')
    const valText = document.getElementById('val-current')
    if (!bar || !valText) return

    let percent
    if (value >= 0) {
        percent = (value / maxCharge) * 100
        bar.classList.remove('discharging')
        bar.classList.add('charging')
    } else {
        percent = (Math.abs(value) / maxDischarge) * 100
        bar.classList.remove('charging')
        bar.classList.add('discharging')
    }

    // clamp [0..100]
    percent = Math.min(100, Math.max(0, percent))

    bar.style.bottom = '0'
    bar.style.height = percent + '%'

    valText.textContent = Number(value).toFixed(1)

    /*     if (value > 0) {
            valText.style.color = '#4CAF50'
        } else if (value < 0) {
            valText.style.color = '#F43662'
        } else {
            valText.style.color = ''
        } */
}

function createCurrentScale() {
    const scaleContainer = document.getElementById('scale-current')
    if (!scaleContainer) return
    scaleContainer.innerHTML = ''

    const positions = [25, 50, 75, 100]

    positions.forEach(pct => {
        const mark = document.createElement('div')
        mark.className = 'scale-mark'
        mark.style.bottom = pct + '%'

        const text = document.createElement('span')
        text.textContent = pct
        mark.appendChild(text)

        scaleContainer.appendChild(mark)
    })
}


window.addEventListener('load', () => {
    container = document.getElementById('stacked-battery-bar')
    addReferenceLabels()
    setupSegments()

    buildRpmGaugeDual()
    updateRpmGaugeDual(0, 0)

    createScale('avCons', 15, [1, 5, 10, 15])
    updateSolidBar('avCons', 0, 15)

    createScale('tank', 45, [5, 15, 25, 35, 45])
    updateSolidBar('tank', 0, 45)

    createScale('invTemp', 100, [20, 40, 60, 80, 100])
    updateSolidBar('invTemp', 0, 100)

    createScale('temp', 120, [20, 40, 60, 80, 100, 120])
    updateSolidBar('temp', 0, 120)

    createCurrentScale()
    updateCurrentBar(0, 100, 100)

    button.addEventListener('click', resetAvSpeed)

    return;
    // Test data 
    updateBatteryBars('172,178,170,170,170,170,170,170,170,170,170,170,170,180,128', -19)
    updateCurrentBar(-19, 100, 100)
    updateSolidBar('temp', 91, 120)
    updateSolidBar('invTemp', 20, 100)
    updateSolidBar('tank', 15, 45)
    updateSolidBar('avCons', 7, 15)
    updateRpmGaugeDual(2200, 90)
    setHtml(ccSpeedElement, 'ccSpeedHtml', '&nbsp;<svg class="ikona"><use xlink:href="graphics.svg#cc"></use></svg>' + 123)
    setHtml(ccSpeedElement, '')
    setText(speedElement, 'speed', 10)
    setText(textNode, 'tripCounter', '#1234')
    updateSteeringVisual(-100)
    setText(timeHMElement, 'timeHM', msToTime(200000))
    setText(tripKmElement, 'tripKm', 12.5)
    setText(avSpeedElement, 'avSpeed', 89)
    setHtml(avSpeedRElement, 'avSpeedR', 69)
})


function updateSteeringVisual(steeringAngle) {

    if (!isDirty('steeringAngle', steeringAngle)) return

    const leftTire = document.getElementById('left-tire-group')
    const rightTire = document.getElementById('right-tire-group')
    const tires = document.querySelectorAll('.tire')
    const treads = document.querySelectorAll('.tire-tread')

    let visualAngle = (steeringAngle / 540) * -35
    visualAngle = Math.max(-35, Math.min(35, visualAngle))

    const isTurning = visualAngle > 5 || visualAngle < -5

    const fillColor = isTurning ? 'rgba(244, 139, 54, 0.6)' : 'none'
    const strokeColor = isTurning ? 'rgba(244, 139, 54, 1)' : '#90E8CE'

    tires.forEach(t => {
        if (t.style.fill !== fillColor) t.style.fill = fillColor
        if (t.style.stroke !== strokeColor) t.style.stroke = strokeColor
    })

    treads.forEach(tr => {
        if (tr.style.stroke !== strokeColor) tr.style.stroke = strokeColor
    })

    const rotateString = `rotate(${visualAngle}deg)`
    if (leftTire) leftTire.style.transform = rotateString
    if (rightTire) rightTire.style.transform = rotateString
}

function resetAvSpeed() {
    msecReset = msec
    distReset = distance
}

function updatePriusColor(avSpeed) {
    const filter = avSpeed > 130
        ? "invert(0.32) sepia(77%) saturate(3814%) hue-rotate(329deg) brightness(101%) contrast(91%)"
        : "invert(0.15) sepia(1) saturate(4) hue-rotate(130deg)"
    setStyle(priusRElement, 'priusFilter', 'filter', filter)
}

function msToTime(milliseconds) {
    var hours = milliseconds / (1000 * 60 * 60)
    var absoluteHours = Math.floor(hours)
    var h = absoluteHours > 9 ? absoluteHours : '0' + absoluteHours
    var minutes = (hours - absoluteHours) * 60
    var absoluteMinutes = Math.floor(minutes)
    var m = absoluteMinutes > 9 ? absoluteMinutes : '0' + absoluteMinutes
    var seconds = (minutes - absoluteMinutes) * 60
    var absoluteSeconds = Math.floor(seconds)
    var s = absoluteSeconds > 9 ? absoluteSeconds : '0' + absoluteSeconds
    return h == "00" ? m + ':' + s : h + ':' + m + ':' + s
}



if (!!window.EventSource) {
    var source
    var lastDataTime = Date.now()

    function setupEventSource() {
        if (source) {
            source.close()
        }
        source = new EventSource('/events')
        source.addEventListener('new_readings', function (e) {
            lastDataTime = Date.now()
            let jsonObj
            try {
                jsonObj = JSON.parse(e.data)
            } catch (err) {
                textNode.textContent = 'parsing ERROR'
                return
            }


            const current = jsonObj.current * -1

            updateBatteryBars(jsonObj.battV, current)

            updateSolidBar('tank', jsonObj.tank, 45)
            updateSolidBar('temp', jsonObj.temp, 120)
            updateSolidBar('invTemp', jsonObj.invTemp, 110)

            const cLoadVal = jsonObj.cl * 20 / 51

            updateRpmGaugeDual(jsonObj.rpm, cLoadVal)

            distance = jsonObj.dist
            const consLpH = jsonObj.cons
            const angle = jsonObj.sAng
            msec = jsonObj.msec
            setText(textNode, 'tripCounter', '#' + jsonObj.tripCounter.toFixed(0))

            const avConsVal = distance === 0 ? 0 : (100 * consLpH / distance)
            updateSolidBar('avCons', avConsVal, 15)

            setText(timeHMElement, 'timeHM', msToTime(msec))
            setText(tripKmElement, 'tripKm', distance.toFixed(1))
            setText(avSpeedElement, 'avSpeed', (3600000 * distance / msec).toFixed())

            if (distReset !== 0) {
                const tempSpeed = 3600000 * (distance - distReset) / (msec - msecReset)
                setHtml(avSpeedRElement, 'avSpeedR', tempSpeed.toFixed())
                updatePriusColor(tempSpeed)
            }

            speed = jsonObj.speed

            updateCurrentBar(current, 100, 100)

            ccSpeed = jsonObj.ccSpeed
            setText(speedElement, 'speed', speed.toFixed())
            const speedColor =
                speed > 131 ? '#f43662'
                    : speed > 55 ? '#f48b36'
                        : '#90e8ce'
            setStyle(speedElement, 'speedColor', 'color', speedColor)

            const ccHtml = ccSpeed > 0
                ? '&nbsp;<svg class="ikona" style="vertical-align:middle"><use xlink:href="graphics.svg#cc"></use></svg>' + ccSpeed.toFixed() 
                : ''
            setHtml(ccSpeedElement, 'ccSpeedHtml', ccHtml)

            setText(roomTempElement, 'roomTemp', (jsonObj.tempRoomC).toFixed())
            setText(numSatElement, 'numSat', (jsonObj.numSat).toFixed())

            setText(dteElement, 'dte',
                (jsonObj.tank * (distance + 100) / (consLpH + 5.5)).toFixed())

            let ang = angle
            if (ang > 1000) ang = ang - 4095
            updateSteeringVisual(ang)

        }, false)

        source.onerror = function () {
            console.log("SSE Error, reconnecting...")
            source.close()
        }
    }

    setupEventSource()
    setInterval(function () {
        const now = Date.now()
        if (now - lastDataTime > 4000) {
            console.log("Konekcija zaleđena, restartujem...")
            lastDataTime = now
            setupEventSource()
        }
    }, 2000)
}

function updateBatteryBars(voltagesStr, current) {
    if (!voltagesStr) return

    if (!isDirtyStr('battV-raw', voltagesStr)) return

    const allVoltages = voltagesStr.split(',')
        .map(v => parseFloat(v.trim()) / 10)
        .filter(v => !isNaN(v))
    const realVoltages = allVoltages.slice(0, 14)

    if (allVoltages.length > 14 && isDirty('auxV', allVoltages[14], 0.05)) {
        auxVoltageElement.textContent = allVoltages[14].toFixed(1)
    }

    const realTotalVoltage = realVoltages.reduce((sum, v) => sum + v, 0)
    if (totalVoltageElement && isDirty('totalV', realTotalVoltage, 0.05)) {
        totalVoltageElement.textContent = realTotalVoltage.toFixed(1)
    }

    // --- Pack imbalance (peak tracking) ---
    if (realVoltages.length >= 2) {
        let maxV = -Infinity, minV = Infinity
        let maxIdx = 0, minIdx = 0
        for (let i = 0; i < realVoltages.length; i++) {
            const v = realVoltages[i]
            if (v > maxV) { maxV = v; maxIdx = i }
            if (v < minV) { minV = v; minIdx = i }
        }
        const deltaV = maxV - minV
        const deltaPct = (deltaV / minV) * 100

        const CURRENT_DEADBAND = 2.0   // A
        if (typeof current === 'number') {
            const sign = current > CURRENT_DEADBAND ? 1
                : current < -CURRENT_DEADBAND ? -1
                    : 0
            if (sign !== 0) {
                if (_cache.lastCurrentSign !== undefined
                    && _cache.lastCurrentSign !== 0
                    && _cache.lastCurrentSign !== sign) {
                    _cache.peakDeltaV = 0
                    _cache.peakDeltaPct = 0
                    _cache.peakWeakIdx = -1
                    _cache.peakStrongIdx = -1
                }
                _cache.lastCurrentSign = sign
            }
        }

        if (deltaV > (_cache.peakDeltaV || 0)) {
            _cache.peakDeltaV = deltaV
            _cache.peakDeltaPct = deltaPct
            _cache.peakWeakIdx = minIdx
            _cache.peakStrongIdx = maxIdx
        }

        const showDeltaV = _cache.peakDeltaV || deltaV
        const showDeltaPct = _cache.peakDeltaPct || deltaPct
        const showWeakIdx = _cache.peakWeakIdx >= 0 ? _cache.peakWeakIdx : minIdx
        const showStrongIdx = _cache.peakStrongIdx >= 0 ? _cache.peakStrongIdx : maxIdx
        //const html = `Δ ${showDeltaV.toFixed(2)}V&nbsp; <br>${showDeltaPct.toFixed(1)}%&nbsp; B${showWeakIdx + 1}`
        const html = `Δ ${showDeltaV.toFixed(1)}V&nbsp; ${showDeltaPct.toFixed(1)}%<br> ${showStrongIdx + 1} <-> ${showWeakIdx + 1}`
        setHtml(packImbalanceElement, 'packImbalanceHtml', html)
    }

    let currentPixelHeight = 0
    realVoltages.forEach((voltage, index) => {
        const segment = segmentElements[index]
        if (!segment) return
        const blockFill = Math.max(0, Math.min(1, (voltage - 12) / 8))
        let segmentHeightPx = blockFill * 16
        segment.cell.style.bottom = `${currentPixelHeight + 1}px`
        segment.cell.style.height = `${segmentHeightPx.toFixed(2)}px`
        currentPixelHeight += segmentHeightPx + 1

        let newColor
        if (voltage < 14.0) newColor = '#F43662'
        else if (voltage > 18.0) newColor = '#FF9800'
        else newColor = 'rgba(145, 232, 206, 0.8)'
        if (isDirtyStr('segColor-' + index, newColor)) {
            segment.cell.style.backgroundColor = newColor
        }
    })

    const newBorder = realTotalVoltage < 196 ? '#FF9800' : '#90E8CE'
    if (isDirtyStr('hvBorder', newBorder)) {
        container.style.borderColor = newBorder
    }
}

function setupSegments() {
    for (let i = 0; i < 14; i++) {
        const cell = document.createElement('div')
        cell.classList.add('segment-cell')
        cell.style.height = '0px'
        cell.style.bottom = '0px'
        container.appendChild(cell)
        segmentElements.push({ cell })
    }
}

function addReferenceLabels() {
    if (!container) return

    const referencePoints = [
        { p: 210 },
        { p: 152 },
        { p: 91 }
    ]

    referencePoints.forEach(point => {
        const topPosition = 238 - point.p
        const tick = document.createElement('div')
        tick.classList.add('bar-tick-line')
        tick.style.top = `${topPosition - 5}px`
        container.appendChild(tick)
    })
}
