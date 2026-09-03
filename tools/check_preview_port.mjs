// visual-preview.html carries a hand port of BreathScene. This runs both over
// the same timeline and fails if they have drifted apart.
//
//   make -C tools check-preview
import { readFileSync, writeFileSync, unlinkSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const html = readFileSync(new URL('../visual-preview.html', import.meta.url), 'utf8');
const script = html.split('<script>')[1].split('</script>')[0];
// Everything above the drawing section is the port; the rest needs a DOM.
const port = script.split('// Drawing')[0].split(
  '// ---------------------------------------------------------------------------').slice(0, -1).join(
  '// ---------------------------------------------------------------------------');

const driver = `
scene.lastAt = 0; scene.stateAt = 0; scene.lastPresence = 0;
let requested = false;
let tapped = false;
const rows = [];
for (let now = 40; now <= 90000; now += 40) {
  if (!requested && now >= 20000) { requested = true; requestSession(now, true); }
  // Inviting waits for a tap rather than starting itself; answer it once the
  // ring has had a couple of seconds to settle, so the timeline still
  // exercises guiding and releasing too.
  if (!tapped && now >= 27200) { tapped = true; handleTap(now); }
  step(now);
  if (now % 400 === 0) {
    const f = scene.field;
    rows.push([now, f.ringRadius.toFixed(3), f.ringLevel.toFixed(4),
               f.coreLevel.toFixed(4), f.horizonLevel.toFixed(4),
               scene.textOpacity.toFixed(4)].join(','));
  }
}
export const csv = rows.join('\\n');
`;

const modulePath = join(tmpdir(), `breathe-port-${process.pid}.mjs`);
writeFileSync(modulePath, port + driver);
let browser;
try {
  ({ csv: browser } = await import('file://' + modulePath));
} finally {
  unlinkSync(modulePath);
}

const firmware = execFileSync(new URL('preview_port_dump', import.meta.url).pathname,
                              { encoding: 'utf8' }).trim();

const a = browser.split('\n'), b = firmware.split('\n');
if (a.length !== b.length) {
  console.error(`row count differs: preview ${a.length}, firmware ${b.length}`);
  process.exit(1);
}
const names = ['t', 'ring radius', 'ring level', 'core level', 'horizon level',
               'text opacity'];
const tolerance = [0, 0.05, 0.002, 0.002, 0.002, 0.002];
let worst = 0;
for (let row = 0; row < a.length; row++) {
  const left = a[row].split(','), right = b[row].split(',');
  for (let col = 1; col < names.length; col++) {
    const delta = Math.abs(parseFloat(left[col]) - parseFloat(right[col]));
    worst = Math.max(worst, delta / tolerance[col]);
    if (delta > tolerance[col]) {
      console.error(`${names[col]} differs at t=${left[0]}ms: ` +
                    `preview ${left[col]}, firmware ${right[col]}`);
      process.exit(1);
    }
  }
}
console.log(`visual-preview.html matches the firmware ` +
            `(worst ${(worst * 100).toFixed(1)}% of tolerance)`);
