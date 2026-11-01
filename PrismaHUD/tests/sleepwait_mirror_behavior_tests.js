const assert = require('assert');
const fs = require('fs');
const vm = require('vm');

const root = __dirname + '/..';
const html = fs.readFileSync(root + '/assets/views/hud_sleepwait_mirror.html', 'utf8');
const script = html.match(/<script>\s*([\s\S]*?)\s*<\/script>/)[1];
const main = fs.readFileSync(root + '/src/main.cpp', 'utf8');

const elements = new Map();
for (const id of ['panel', 'title', 'hours', 'range', 'mode', 'active', 'paused', 'probe']) {
    elements.set(id, { textContent: '', className: '' });
}

let intervalStarts = 0;
let intervalClears = 0;
const context = {
    window: {},
    document: { getElementById: id => elements.get(id) },
    JSON,
    Number,
    String,
    setInterval: () => { intervalStarts += 1; return intervalStarts; },
    clearInterval: () => { intervalClears += 1; },
};
vm.runInNewContext(script, context);

assert.strictEqual(intervalStarts, 0, 'closed mirror must not start a native poll interval');
context.window.updateSleepWaitMirror({ visible: true, hours: null, maxHours: null });
assert.strictEqual(elements.get('hours').textContent, '?', 'null hours must stay unresolved');
assert.match(elements.get('range').textContent, /NOT YET RESOLVED/, 'null hours must show the unresolved warning');
assert.strictEqual(intervalStarts, 1, 'visible mirror must start polling');
context.window.updateSleepWaitMirror({ visible: false });
assert.strictEqual(intervalClears, 1, 'hidden mirror must stop polling');

const suppressor = main.slice(main.indexOf('static bool IsSuppressingMenuName'), main.indexOf('class MenuSuppressSink'));
assert(!suppressor.includes('SleepWaitMenu'), 'read-only SleepWait must not use the raw menu suppressor');

console.log('PrismaHUD SleepWait mirror behavior tests passed');
