// PKJS companion for Pixel Active watchface.
// Integrates with the Pebble Complications / Plugins SDK (preferences/complications + enumeratePlugins)
// and bundled config.html for Pixel Watch Active theme & slot customization.

var SLOT_COUNT = 5;

var ICON_NONE = 0;
var ICON_STEPS = 1;
var ICON_HEART = 2;
var ICON_SUN = 3;
var ICON_CLOUD = 4;
var ICON_RAIN = 5;
var ICON_BATTERY = 6;
var ICON_FLAME = 7;
var ICON_PIN = 8;
var ICON_CALENDAR = 9;

var DEFAULT_CONFIG = {
  theme: 0,
  showDot: true,
  leadingZero: true,
  timeFont: 0,
  stackedTime: false,
  // Each slot: { mode: 'auto' | 'custom' | 'local' | 'none', category, item, index, property, shape }
  slots: [
    { mode: 'auto' }, // 0: Top Ring (Preferred #1)
    { mode: 'auto' }, // 1: Mid Ring (Preferred #2)
    { mode: 'auto' }, // 2: Bot Ring (Preferred #3)
    { mode: 'auto' }, // 3: Top-Left Badge (Preferred #4)
    { mode: 'auto' }, // 4: Bot-Left Badge (Preferred #5)
  ],
};

var config = loadConfig();
var imageSize = 20;
var preferred = [];
var subscriptions = [];
var sentPixels = [];
var livePreviews = {};

function log(msg) {
  console.log('[PixelActive] ' + msg);
}

function loadConfig() {
  try {
    var raw = localStorage.getItem('pixelActiveConfig');
    if (raw) {
      var parsed = JSON.parse(raw);
      var merged = Object.assign({}, DEFAULT_CONFIG, parsed);
      if (!Array.isArray(merged.slots) || merged.slots.length < SLOT_COUNT) {
        merged.slots = DEFAULT_CONFIG.slots.slice();
      }
      return merged;
    }
  } catch (e) {
    log('loadConfig error: ' + e);
  }
  return JSON.parse(JSON.stringify(DEFAULT_CONFIG));
}

function saveConfig() {
  try {
    localStorage.setItem('pixelActiveConfig', JSON.stringify(config));
  } catch (e) {
    log('saveConfig error: ' + e);
  }
}

function textOf(payloads) {
  return payloads && payloads.shortText ? payloads.shortText.text : '';
}

function bytesOf(base64) {
  var binary = atob(base64);
  var bytes = [];
  for (var i = 0; i < binary.length; i++) bytes.push(binary.charCodeAt(i));
  return bytes;
}

function inferIconHint(reading, instance) {
  var prop = (reading.property || '').toLowerCase();
  var cat = (reading.category || '').toLowerCase();
  var item = (reading.item || '').toLowerCase();

  if (cat === 'weather' && instance && instance.properties) {
    var code = textOf(instance.properties.condition_code).toLowerCase();
    if (code.indexOf('rain') !== -1) return ICON_RAIN;
    if (code.indexOf('cloud') !== -1) return ICON_CLOUD;
    if (code.indexOf('sun') !== -1) return ICON_SUN;
  }
  if (prop.indexOf('step') !== -1) return ICON_STEPS;
  if (prop.indexOf('heart') !== -1 || prop.indexOf('bpm') !== -1) return ICON_HEART;
  if (prop.indexOf('temp') !== -1 || cat === 'weather') return ICON_SUN;
  if (prop.indexOf('batt') !== -1 || item.indexOf('battery') !== -1) return ICON_BATTERY;
  if (prop.indexOf('cal') !== -1 || prop.indexOf('active') !== -1) return ICON_FLAME;
  if (prop.indexOf('dist') !== -1 || cat === 'location') return ICON_PIN;
  if (cat === 'calendar' || prop.indexOf('event') !== -1) return ICON_CALENDAR;
  return ICON_NONE;
}

function declaredShapes(reading) {
  var shapes = [];
  if (typeof Pebble.enumeratePlugins !== 'function') return shapes;
  Pebble.enumeratePlugins().forEach(function (plugin) {
    (plugin.sources || []).forEach(function (source) {
      if (source.category !== reading.category || (source.items || []).indexOf(reading.item) === -1) return;
      shapes = shapes.concat((source.properties || {})[reading.property] || []);
    });
  });
  return shapes;
}

function send(message) {
  Pebble.sendAppMessage(message, function () {}, function (e) {
    log('sendAppMessage failed: ' + JSON.stringify(e));
  });
}

function sendSettings() {
  send({
    theme: config.theme | 0,
    showDot: config.showDot ? 1 : 0,
    leadingZero: config.leadingZero ? 1 : 0,
    timeFont: config.timeFont | 0,
    stackedTime: config.stackedTime ? 1 : 0,
  });
}

function labelOf(instance, reading) {
  var name = instance && instance.properties ? textOf(instance.properties.name) : '';
  return name || (reading.property || '').replace(/_/g, ' ').toUpperCase();
}

function onValue(slot, reading, envelope) {
  var instances = envelope.instances || [];
  var instance = instances[reading.index || 0] || instances[0];
  var payloads = instance && instance.properties ? instance.properties[reading.property] : null;
  var number = payloads && payloads.numericValue;
  var text = textOf(payloads) || (number ? Math.round(number.value) + (number.unit || '') : '');
  if (!text) {
    livePreviews[slot] = '--';
    send({
      slot: slot,
      numText: '--',
      numLabel: labelOf(instance, reading),
      numFill: -1,
      iconHint: inferIconHint(reading, instance),
    });
    return;
  }
  var hasRange = number && number.min !== undefined && number.max !== undefined && number.max > number.min;
  var fill = hasRange ? Math.round((number.value - number.min) / (number.max - number.min) * 100) : -1;
  fill = Math.max(-1, Math.min(100, fill));
  livePreviews[slot] = text;
  send({
    slot: slot,
    numText: text.slice(0, 11),
    numLabel: labelOf(instance, reading).slice(0, 13),
    numFill: fill,
    iconHint: inferIconHint(reading, instance),
  });
}

function onImage(slot, reading, envelope) {
  var instances = envelope.instances || [];
  var instance = instances[reading.index || 0] || instances[0];
  var payloads = (instance && instance.properties && instance.properties[reading.property]) || {};
  var bitmap = payloads.image || payloads.icon;
  if (!bitmap) {
    onValue(slot, reading, envelope);
    return;
  }
  if (bitmap.pixels === sentPixels[slot]) return;
  sentPixels[slot] = bitmap.pixels;
  livePreviews[slot] = '[Image]';
  Pebble.sendAppMessage({
    slot: slot,
    imgLabel: labelOf(instance, reading).slice(0, 13),
    iconWidth: bitmap.width,
    iconHeight: bitmap.height,
    iconPalette: bytesOf(bitmap.palette),
    iconPixels: bytesOf(bitmap.pixels),
  }, function () {}, function (e) {
    sentPixels[slot] = '';
    log('bitmap send failed: ' + JSON.stringify(e));
  });
}

function subscribeSlot(slot, reading, preferImage) {
  if (typeof Pebble.subscribeToSource !== 'function') return;
  var shapes = declaredShapes(reading);
  var useImage = preferImage && (shapes.indexOf('image') !== -1 || shapes.indexOf('icon') !== -1) && slot < SLOT_COUNT;
  var props = ['name', reading.property];
  if (reading.category === 'weather' && props.indexOf('condition_code') === -1) {
    props.push('condition_code');
  }
  var req = {
    category: reading.category,
    item: reading.item,
    properties: props,
    onData: function (envelope) {
      if (useImage) onImage(slot, reading, envelope);
      else onValue(slot, reading, envelope);
    },
    onError: function (err) {
      log('slot ' + slot + ' (' + reading.category + '/' + reading.item + '): ' + err.code);
      livePreviews[slot] = err.code;
    },
  };
  if (useImage) req.iconPixelSize = { w: imageSize, h: imageSize };
  subscriptions.push(Pebble.subscribeToSource(req));
}

function resolveAutoReading(slotIndex) {
  if (slotIndex < preferred.length) {
    return preferred[slotIndex];
  }
  return null;
}

function clearSlot(i, label) {
  send({ slot: i, numText: '', numLabel: '', numFill: -1, iconHint: ICON_NONE });
  livePreviews[i] = label;
}

function subscribeAll() {
  subscriptions.forEach(function (s) {
    try { s.unsubscribe(); } catch (e) {}
  });
  subscriptions = [];
  sentPixels = [];

  for (var i = 0; i < SLOT_COUNT; i++) {
    var sc = config.slots[i] || { mode: 'auto' };
    if (sc.mode === 'none') {
      clearSlot(i, 'Hidden');
    } else if (sc.mode === 'local') {
      var tag = i === 3 ? 'BAT' : (i === 4 ? 'DATE' : 'STEPS');
      send({ slot: i, numText: '', numLabel: tag, numFill: -1, iconHint: ICON_NONE });
      livePreviews[i] = 'Built-in';
    } else if (sc.mode === 'custom' && sc.category && sc.item && sc.property) {
      var wantImg = sc.shape === 'image' || sc.shape === 'icon';
      subscribeSlot(i, sc, wantImg);
    } else if (sc.mode === 'auto') {
      var reading = resolveAutoReading(i);
      if (reading) {
        var shapes = declaredShapes(reading);
        var hasVal = shapes.indexOf('numericValue') !== -1 || shapes.indexOf('shortText') !== -1;
        var hasImg = shapes.indexOf('image') !== -1 || shapes.indexOf('icon') !== -1;
        var preferImg = (i === 3 || i === 4) ? hasImg : (!hasVal && hasImg);
        subscribeSlot(i, reading, preferImg);
      } else {
        clearSlot(i, 'Empty (hidden)');
      }
    }
  }
}

function onPreferred(envelope) {
  preferred = (envelope.instances || []).map(function (instance) {
    var p = instance.properties || {};
    return {
      category: textOf(p.category),
      item: textOf(p.item),
      index: p.index && p.index.numericValue ? p.index.numericValue.value : 0,
      property: textOf(p.property),
    };
  });
  subscribeAll();
}

function buildCatalogue() {
  if (typeof Pebble.enumeratePlugins !== 'function') return [];
  var out = [];
  Pebble.enumeratePlugins().forEach(function (plugin) {
    (plugin.sources || []).forEach(function (source) {
      (source.items || []).forEach(function (item) {
        out.push({
          pluginName: plugin.name || 'Plugin',
          category: source.category,
          item: item,
          properties: source.properties || {},
        });
      });
    });
  });
  return out;
}

Pebble.addEventListener('configmessage', function (e) {
  var msg = e.data || {};
  if (msg.type === 'init') {
    e.respond({
      config: config,
      catalogue: buildCatalogue(),
      previews: livePreviews,
    });
    return;
  }
  if (msg.type === 'setAppearance') {
    if (msg.theme !== undefined) config.theme = msg.theme | 0;
    if (msg.showDot !== undefined) config.showDot = !!msg.showDot;
    if (msg.leadingZero !== undefined) config.leadingZero = !!msg.leadingZero;
    if (msg.timeFont !== undefined) config.timeFont = msg.timeFont | 0;
    if (msg.stackedTime !== undefined) config.stackedTime = !!msg.stackedTime;
    saveConfig();
    sendSettings();
    e.respond({ ok: true });
    return;
  }
  if (msg.type === 'setSlot') {
    var idx = msg.index | 0;
    if (idx >= 0 && idx < SLOT_COUNT && msg.slotConfig) {
      config.slots[idx] = msg.slotConfig;
      saveConfig();
      subscribeAll();
    }
    e.respond({ ok: true, previews: livePreviews });
    return;
  }
  if (msg.type === 'reset') {
    config = JSON.parse(JSON.stringify(DEFAULT_CONFIG));
    saveConfig();
    sendSettings();
    subscribeAll();
    e.respond({ ok: true, config: config });
  }
});

function hello(retries) {
  Pebble.sendAppMessage({ hello: 1 }, function () {}, function () {
    if (retries === 0) return;
    setTimeout(function () { hello(retries === undefined ? 2 : retries - 1); }, 1000);
  });
}

Pebble.addEventListener('appmessage', function (e) {
  var size = (e.payload || {}).imageSize;
  if (size === undefined || size === imageSize) return;
  imageSize = size;
  subscribeAll();
});

Pebble.addEventListener('ready', function () {
  log('ready');
  hello();
  sendSettings();
  if (typeof Pebble.subscribeToSource === 'function') {
    Pebble.subscribeToSource({
      category: 'preferences',
      item: 'complications',
      onData: onPreferred,
      onError: function (err) {
        log('preferences/complications: ' + err.code);
        subscribeAll();
      },
    });
  } else {
    subscribeAll();
  }
});
