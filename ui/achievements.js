"use strict";

const grades = ["bronze", "silver", "gold", "platinum"];
const tiers = document.querySelector("#tiers");
const summary = document.querySelector("#summary");
const notice = document.querySelector("#notice");
const refreshButton = document.querySelector("#refresh");
let catalog = [];
let illustrated = new Set();
let saved = new Map();
let requestId = 0;

function launcherOrigin() {
  const origins = ["http://tauri.localhost", "https://tauri.localhost", "tauri://localhost"];
  const ancestor = location.ancestorOrigins?.[0];
  if (origins.includes(ancestor)) return ancestor;
  // The launcher's iframe deliberately omits the referrer. Match the scheme
  // used by its custom protocol on Windows, where this package runs.
  if (location.hostname === "openrac-mod.localhost") return `${location.protocol}//tauri.localhost`;
  return "";
}

const host = launcherOrigin();

function loadStoredTrophies() {
  return new Promise((resolve, reject) => {
    if (!host) {
      reject(new Error("Launcher connection unavailable"));
      return;
    }
    const id = ++requestId;
    const timer = setTimeout(() => finish(reject, new Error("Launcher did not respond")), 2000);
    function finish(callback, value) {
      clearTimeout(timer);
      window.removeEventListener("message", receive);
      callback(value);
    }
    function receive(event) {
      const message = event.data;
      if (event.source !== parent || event.origin !== host || message?.openrac !== 1 || message.id !== id) return;
      if (message.ok === true) finish(resolve, message.value);
      else finish(reject, new Error("Could not read trophy progress"));
    }
    window.addEventListener("message", receive);
    parent.postMessage({ openrac: 1, id, call: "storeGet", args: { key: "trophies" } }, host);
  });
}

function collectionEntries(value) {
  if (value?.version !== 1 || !Array.isArray(value.trophies) || value.trophies.length !== 35) {
    throw new Error("Invalid trophy collection");
  }
  const ids = new Set();
  for (const trophy of value.trophies) {
    if (typeof trophy?.id !== "string" || !/^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(trophy.id) ||
      ids.has(trophy.id)) throw new Error("Invalid trophy ID");
    ids.add(trophy.id);
  }
  return value.trophies;
}

function readCatalog(value) {
  return collectionEntries(value).map(({ id, name, description, grade }) => {
    if (typeof name !== "string" || typeof description !== "string" || !grades.includes(grade)) {
      throw new Error("Invalid trophy description or grade");
    }
    return { id, name, description, grade };
  });
}

function readProgress(value) {
  if (value === null) return new Map();
  const entries = new Map();
  for (const { id, earned, progress } of collectionEntries(value)) {
    if (typeof earned !== "boolean") throw new Error("Invalid earned status");
    if (progress !== null && (!Number.isSafeInteger(progress?.current) || !Number.isSafeInteger(progress.total) ||
      progress.total <= 0 || progress.current < 0 || progress.current > progress.total)) {
      throw new Error("Invalid trophy progress");
    }
    entries.set(id, { earned, progress });
  }
  if (catalog.some(trophy => !entries.has(trophy.id))) throw new Error("Progress does not match this catalog");
  return entries;
}

function textElement(tag, content, className) {
  const element = document.createElement(tag);
  if (className) element.className = className;
  element.textContent = content;
  return element;
}

async function loadIllustratedTrophies() {
  try {
    const response = await fetch("icons/index.json", { credentials: "same-origin" });
    const ids = response.ok ? await response.json() : null;
    if (Array.isArray(ids)) return new Set(ids);
  } catch {
    // The optional local art can be absent. Tier medals are always available.
  }
  return new Set();
}

function card(trophy) {
  const state = saved.get(trophy.id);
  const element = document.createElement("article");
  element.className = `card ${state?.earned ? "earned" : "locked"} ${trophy.grade}`;
  const icon = textElement("span", state?.earned ? "★" : "◇", `medal ${trophy.grade}`);
  icon.setAttribute("aria-hidden", "true");
  if (illustrated.has(trophy.id)) {
    const image = document.createElement("img");
    image.className = "trophy-art";
    image.alt = "";
    image.decoding = "async";
    image.addEventListener("load", () => icon.replaceWith(image), { once: true });
    image.src = `icons/${trophy.id}.png`;
  }
  const body = document.createElement("div");
  const meta = document.createElement("div");
  meta.className = "meta";
  meta.append(textElement("span", state?.earned ? "Earned" : "Locked"), textElement("span", trophy.grade));
  body.append(textElement("h3", trophy.name), textElement("p", trophy.description), meta);
  if (state?.progress) {
    const { current, total } = state.progress;
    const line = document.createElement("div");
    line.className = "progress-line";
    const bar = document.createElement("progress");
    bar.max = total;
    bar.value = current;
    bar.setAttribute("aria-label", `${trophy.name} progress`);
    line.append(bar, textElement("span", `${current}/${total}`, "progress-value"));
    body.append(line);
  }
  element.append(icon, body);
  if (state?.earned) {
    const shimmer = document.createElement("canvas");
    shimmer.className = "edge-shimmer";
    shimmer.setAttribute("aria-hidden", "true");
    shimmer.setAttribute("data-grade", trophy.grade);
    shimmer.setAttribute("data-trophy", trophy.id);
    element.append(shimmer);
  }
  return element;
}

function render() {
  const earned = catalog.filter(trophy => saved.get(trophy.id)?.earned).length;
  summary.textContent = `${earned} of ${catalog.length} trophies earned`;
  const sections = document.createDocumentFragment();
  for (const grade of grades) {
    const group = catalog.filter(trophy => trophy.grade === grade);
    const section = document.createElement("section");
    section.className = "tier";
    section.append(textElement("h2", grade[0].toUpperCase() + grade.slice(1)));
    const grid = document.createElement("div");
    grid.className = "grid";
    grid.append(...group.map(card));
    section.append(grid);
    sections.append(section);
  }
  tiers.replaceChildren(sections);
  globalThis.achievementShimmer?.observe(tiers);
}

async function refresh() {
  if (!catalog.length || refreshButton.disabled) return;
  refreshButton.disabled = true;
  try {
    saved = readProgress(await loadStoredTrophies());
    notice.textContent = saved.size ? "" : "Load a save with the mod enabled to see your progress here.";
    render();
  } catch {
    notice.textContent = "Progress could not be refreshed. Try opening the page again from the launcher.";
  } finally {
    refreshButton.disabled = false;
  }
}

async function start() {
  try {
    const [response, icons] = await Promise.all([
      fetch("catalog.json", { credentials: "same-origin" }),
      loadIllustratedTrophies(),
    ]);
    if (!response.ok) throw new Error("Catalog unavailable");
    catalog = readCatalog(await response.json());
    illustrated = icons;
    render();
    refreshButton.disabled = false;
    await refresh();
  } catch {
    summary.textContent = "Trophy catalog could not be loaded.";
  }
}

refreshButton.addEventListener("click", refresh);
window.addEventListener("message", event => {
  const message = event.data;
  if (event.source !== parent || event.origin !== host || message?.openrac !== 1 || message.event !== "pad" || !message.down) return;
  if (message.button === "up" || message.button === "down") {
    window.scrollBy(0, message.button === "up" ? -160 : 160);
  }
});
void start();
