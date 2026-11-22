/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Visual reference: Vincent Garreau's particles.js NASA preset.
 * https://vincentgarreau.com/particles.js/#nasa
 * Original canvas renderer; no particles.js library or preset code is bundled.
 */
(() => {
  const hero = document.querySelector('.gloam-hero');
  if (!hero) return;
  const canvas = document.createElement('canvas');
  const context = canvas.getContext('2d');
  if (!context) return;
  canvas.className = 'gloam-starfield';
  canvas.setAttribute('aria-hidden', 'true');
  hero.prepend(canvas);

  const motion = matchMedia('(prefers-reduced-motion: reduce)');
  // Track change events once; the frame loop only reads this cached preference.
  let reduceMotion = motion.matches;
  const connection = navigator.connection;
  const controls = hero.querySelector('.gloam-hero-controls');
  const finePointer = matchMedia('(pointer: fine)');
  const palette = ['198,183,255', '244,160,229', '126,221,244', '255,220,166'];
  const frameInterval = 1000 / 30;
  const storageKey = 'gloam-stars-paused';
  let paused = false;
  try { paused = localStorage.getItem(storageKey) === 'true'; } catch (_) { /* Optional. */ }

  const toggle = document.createElement('button');
  toggle.type = 'button';
  toggle.className = 'gloam-star-toggle';
  hero.append(toggle);
  let stars = [], width = 0, height = 0, ratio = 1;
  let frame = 0, previous = 0, painted = 0, elapsed = 0;
  let inView = false, away = false;
  let pointerX = 0, pointerY = 0, targetX = 0, targetY = 0;
  let quiet = {left: 0, right: 0, top: 0, bottom: 0};

  // Shared sprites keep gradients and four-point geometry out of the frame loop.
  const glints = palette.map(color => {
    const sprite = document.createElement('canvas');
    sprite.width = sprite.height = 64;
    const paint = sprite.getContext('2d');
    if (!paint) return null;
    const glow = paint.createRadialGradient(32, 32, 0, 32, 32, 32);
    glow.addColorStop(0, `rgba(${color},.65)`);
    glow.addColorStop(.3, `rgba(${color},.18)`);
    glow.addColorStop(1, `rgba(${color},0)`);
    paint.fillStyle = glow;
    paint.fillRect(0, 0, 64, 64);
    paint.fillStyle = `rgb(${color})`;
    paint.beginPath();
    for (const [x, y] of [[32,7],[35,28],[57,32],[35,36],[32,57],[29,36],[7,32],[29,28]]) {
      paint.lineTo(x, y);
    }
    paint.closePath();
    paint.fill();
    paint.fillStyle = '#fff8ff';
    paint.beginPath();
    paint.arc(32, 32, 2, 0, Math.PI * 2);
    paint.fill();
    return sprite;
  });

  const still = () => reduceMotion || Boolean(connection && connection.saveData);
  function stop() {
    if (frame) cancelAnimationFrame(frame);
    frame = 0;
    previous = painted = 0;
  }
  function draw(delta) {
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);
    pointerX += (targetX - pointerX) * Math.min(1, delta * 2.5);
    pointerY += (targetY - pointerY) * Math.min(1, delta * 2.5);
    for (const star of stars) {
      star.x = (star.x + star.vx * delta / width + 1) % 1;
      star.y = (star.y + star.vy * delta / height + 1) % 1;
      const x = star.x * width + pointerX * star.depth;
      const y = star.y * height + pointerY * star.depth;
      const wave = (Math.sin(star.phase + elapsed * star.speed) + 1) / 2;
      const pulse = wave * wave * wave;
      const distance = Math.hypot((x - width / 2) / Math.max(180, width * .24),
                                  (y - height * .39) / Math.max(120, height * .42));
      const centerFade = .2 + .8 * Math.min(1, distance);
      const behindControls = x > quiet.left && x < quiet.right && y > quiet.top && y < quiet.bottom;
      context.globalAlpha = (.13 + pulse * .7) * centerFade * (behindControls ? .12 : 1);
      if (star.glint && glints[star.color]) {
        const size = 8 + pulse * 13;
        context.drawImage(glints[star.color], x - size / 2, y - size / 2, size, size);
      } else {
        context.fillStyle = star.ink;
        context.beginPath();
        context.arc(x, y, star.size * (.8 + pulse * .35), 0, Math.PI * 2);
        context.fill();
      }
    }
    context.globalAlpha = 1;
  }
  function tick(now) {
    frame = 0;
    if (!inView || document.hidden || away || paused || still()) return;
    if (now - painted >= frameInterval) {
      const delta = previous ? Math.min((now - previous) / 1000, .1) : 0;
      previous = now;
      painted = now - ((now - painted) % frameInterval);
      elapsed += delta;
      draw(delta);
    }
    frame = requestAnimationFrame(tick);
  }
  function synchronize() {
    toggle.hidden = still();
    toggle.textContent = paused ? 'Resume stars' : 'Pause stars';
    toggle.setAttribute('aria-pressed', String(paused));
    if (!inView || document.hidden || away) { stop(); return; }
    if (paused || still()) { stop(); draw(0); return; }
    if (!frame) frame = requestAnimationFrame(tick);
  }
  function resize() {
    const box = hero.getBoundingClientRect();
    width = Math.max(1, box.width);
    height = Math.max(1, box.height);
    // Bound the backing store as well as particle count on large/retina displays.
    ratio = Math.min(devicePixelRatio || 1, 1.5, Math.sqrt(2000000 / (width * height)));
    canvas.width = Math.max(1, Math.floor(width * ratio));
    canvas.height = Math.max(1, Math.floor(height * ratio));
    if (controls) {
      const area = controls.getBoundingClientRect();
      quiet = {left: area.left - box.left - 20, right: area.right - box.left + 20,
               top: area.top - box.top - 12, bottom: area.bottom - box.top + 12};
    }
    const count = Math.min(160, Math.max(42, Math.round(width * height / 6500)));
    stars = Array.from({length: count}, (_, index) => ({
      x: Math.random(), y: Math.random(), color: index % palette.length,
      ink: `rgb(${palette[index % palette.length]})`,
      glint: index % 5 === 0, size: .45 + Math.random() * .8,
      phase: Math.random() * Math.PI * 2, speed: .6 + Math.random() * 1.2,
      vx: -1.5 + Math.random() * 3, vy: -.8 + Math.random() * 1.6,
      depth: .3 + Math.random() * .7,
    }));
    synchronize();
  }
  toggle.addEventListener('click', () => {
    paused = !paused;
    try { localStorage.setItem(storageKey, String(paused)); } catch (_) { /* Optional. */ }
    synchronize();
  });
  hero.addEventListener('pointermove', event => {
    if (!finePointer.matches || paused || still()) return;
    const box = hero.getBoundingClientRect();
    targetX = ((event.clientX - box.left) / width - .5) * 10;
    targetY = ((event.clientY - box.top) / height - .5) * 7;
  }, {passive: true});
  hero.addEventListener('pointerleave', () => { targetX = targetY = 0; }, {passive: true});
  document.addEventListener('visibilitychange', synchronize);
  window.addEventListener('pagehide', () => { away = true; stop(); });
  window.addEventListener('pageshow', () => { away = false; synchronize(); });
  const motionChanged = event => { reduceMotion = event.matches; synchronize(); };
  if (motion.addEventListener) motion.addEventListener('change', motionChanged);
  else motion.addListener(motionChanged);
  if (connection && connection.addEventListener) connection.addEventListener('change', synchronize);
  if ('IntersectionObserver' in window) {
    new IntersectionObserver(entries => {
      inView = entries[0].isIntersecting;
      synchronize();
    }).observe(hero);
  } else {
    const checkPosition = () => {
      const box = hero.getBoundingClientRect();
      inView = box.bottom > 0 && box.top < innerHeight;
      synchronize();
    };
    window.addEventListener('scroll', checkPosition, {passive: true});
    checkPosition();
  }
  if ('ResizeObserver' in window) new ResizeObserver(resize).observe(hero);
  else window.addEventListener('resize', resize, {passive: true});
  resize();
})();
