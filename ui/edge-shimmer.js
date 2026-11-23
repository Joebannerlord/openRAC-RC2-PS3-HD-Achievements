"use strict";

globalThis.achievementShimmer = (() => {
  const motion = matchMedia("(prefers-reduced-motion: reduce)");
  const palettes = {
    bronze: { halo: "#d58646", light: "#ffd5a1", reach: 8.2, energy: .79 },
    silver: { halo: "#96c9f6", light: "#ecf8ff", reach: 8.9, energy: .88 },
    gold: { halo: "#ffc25d", light: "#fff2bc", reach: 9.6, energy: 1 },
    platinum: { halo: "#92e9e3", light: "#e5ffff", reach: 10.2, energy: 1 },
  };

  // A small, tapered ray painted once and tinted per tier. Each instance can
  // then stretch and brighten independently as it curls off the edge.
  function makeRay(halo, light) {
    const stamp = document.createElement("canvas");
    stamp.width = 16;
    stamp.height = 28;
    const ctx = stamp.getContext("2d");
    const glow = ctx.createRadialGradient(8, 24, 0, 8, 24, 11);
    glow.addColorStop(0, light + "99");
    glow.addColorStop(.5, halo + "46");
    glow.addColorStop(1, halo + "00");
    ctx.fillStyle = glow;
    ctx.fillRect(0, 0, 16, 28);
    const streak = ctx.createLinearGradient(0, 2, 0, 26);
    streak.addColorStop(0, halo + "00");
    streak.addColorStop(.28, halo + "80");
    streak.addColorStop(.55, light + "b4");
    streak.addColorStop(.88, light);
    streak.addColorStop(1, halo + "25");
    ctx.fillStyle = streak;
    ctx.beginPath();
    ctx.moveTo(6, 26);
    ctx.quadraticCurveTo(9, 16, 10, 2);
    ctx.quadraticCurveTo(11, 16, 9, 26);
    ctx.closePath();
    ctx.fill();
    return stamp;
  }

  for (const palette of Object.values(palettes)) palette.ray = makeRay(palette.halo, palette.light);
  const violetRay = makeRay("#c189eb", "#fce8ff");

  function alongRim(width, height, margin, radius, trophyId) {
    let seed = 0;
    for (const character of trophyId) seed = (Math.imul(seed, 31) + character.charCodeAt(0)) >>> 0;
    function random() {
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
      return seed / 4294967296;
    }

    const points = [];
    function add(x, y, angle) {
      const lean = (random() - .5) * 1.24;
      points.push({
        x, y,
        cos: Math.cos(angle + lean),
        sin: Math.sin(angle + lean),
        phase: (x + y * 1.8) * .12 + random() * 3,
        length: .64 + random() * .52,
        speed: 2.5 + random() * 2.1,
      });
    }

    const horizontal = Math.max(1, Math.ceil((width - 2 * radius) / 5.5));
    const vertical = Math.max(1, Math.ceil((height - 2 * radius) / 5.5));
    for (let i = 0; i < horizontal; i++) {
      const x = margin + radius + (width - 2 * radius) * ((i + .25 + .5 * random()) / horizontal);
      add(x, margin, 0);
      add(x, margin + height, Math.PI);
    }
    for (let i = 0; i < vertical; i++) {
      const y = margin + radius + (height - 2 * radius) * ((i + .25 + .5 * random()) / vertical);
      add(margin, y, -Math.PI / 2);
      add(margin + width, y, Math.PI / 2);
    }
    const corners = [
      [margin + radius, margin + radius, Math.PI],
      [margin + width - radius, margin + radius, Math.PI * 1.5],
      [margin + width - radius, margin + height - radius, 0],
      [margin + radius, margin + height - radius, Math.PI * .5],
    ];
    for (const [cx, cy, start] of corners) {
      for (let i = 0; i < 4; i++) {
        const angle = start + (i + .5) * Math.PI / 8;
        add(cx + radius * Math.cos(angle), cy + radius * Math.sin(angle), angle + Math.PI / 2);
      }
    }
    return points;
  }

  function draw(surface, time) {
    if (!surface.points) return;
    const { ctx, canvas, ratio, palette, path, points, grade } = surface;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    ctx.strokeStyle = palette.halo;
    ctx.globalAlpha = .17 * palette.energy;
    ctx.lineWidth = 5;
    ctx.stroke(path);
    ctx.globalAlpha = .30 * palette.energy;
    ctx.lineWidth = 2.7;
    ctx.stroke(path);
    ctx.strokeStyle = palette.light;
    ctx.globalAlpha = .59 * palette.energy;
    ctx.lineWidth = .9;
    ctx.stroke(path);

    for (const [i, point] of points.entries()) {
      const wave = .5 + .5 * Math.sin(time * point.speed + point.phase);
      const swell = .5 + .5 * Math.sin(time * 1.8 - point.phase * .41);
      const peak = wave * wave * (.46 + .54 * swell);
      const length = palette.reach * point.length * (.58 + .42 * peak);
      const width = 5.2 + 2.4 * peak;
      ctx.setTransform(point.cos * ratio, point.sin * ratio, -point.sin * ratio, point.cos * ratio,
        point.x * ratio, point.y * ratio);
      ctx.globalAlpha = palette.energy * (.42 + .58 * peak);
      const stamp = grade === "platinum" && i % 3 === 0 ? violetRay : palette.ray;
      ctx.drawImage(stamp, -width / 2, .6 - length, width, length);
    }
  }

  const surfaces = new Map();
  const visible = new Set();
  let frame = 0;
  let lastPaint = 0;

  function animate(now) {
    frame = 0;
    if (now - lastPaint >= 1000 / 30) {
      lastPaint = now;
      for (const canvas of visible) draw(surfaces.get(canvas), now / 1000);
    }
    schedule();
  }

  function schedule() {
    if (!frame && visible.size && !motion.matches) frame = requestAnimationFrame(animate);
  }

  const resize = new ResizeObserver((entries) => {
    for (const { target: canvas, contentRect } of entries) {
      const surface = surfaces.get(canvas);
      if (!surface) continue;
      const margin = parseFloat(getComputedStyle(canvas).getPropertyValue("--shimmer-margin"));
      const width = contentRect.width - 2 * margin;
      const height = contentRect.height - 2 * margin;
      if (!(width > 0 && height > 0)) continue;
      const radius = Math.min(12, width / 2, height / 2);
      surface.ratio = Math.min(devicePixelRatio, 2);
      canvas.width = Math.round(contentRect.width * surface.ratio);
      canvas.height = Math.round(contentRect.height * surface.ratio);
      surface.points = alongRim(width, height, margin, radius, canvas.dataset.trophy);
      surface.path = new Path2D();
      surface.path.roundRect(margin, margin, width, height, radius);
      draw(surface, motion.matches ? 0 : performance.now() / 1000);
    }
  });

  const intersect = new IntersectionObserver((entries) => {
    for (const { target, isIntersecting } of entries) {
      if (!surfaces.has(target)) continue;
      if (isIntersecting) visible.add(target);
      else visible.delete(target);
    }
    schedule();
  });

  motion.addEventListener("change", () => {
    if (motion.matches) {
      cancelAnimationFrame(frame);
      frame = 0;
      for (const surface of surfaces.values()) draw(surface, 0);
    } else {
      schedule();
    }
  });

  return {
    observe(root) {
      resize.disconnect();
      intersect.disconnect();
      visible.clear();
      surfaces.clear();
      for (const canvas of root.querySelectorAll(".edge-shimmer")) {
        const grade = canvas.dataset.grade;
        const ctx = canvas.getContext("2d");
        if (!ctx || !palettes[grade]) continue;
        surfaces.set(canvas, { canvas, ctx, grade, palette: palettes[grade] });
        resize.observe(canvas);
        intersect.observe(canvas);
      }
    },
  };
})();
