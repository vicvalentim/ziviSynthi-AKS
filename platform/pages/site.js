(() => {
  const root = document.documentElement;
  const themeKey = "zivi-aks-color-theme";
  const languageKey = "zivi-aks-language";
  const darkMode = matchMedia("(prefers-color-scheme: dark)");
  const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");
  const neutralThemes = new Set(["system", "light", "dark"]);

  function read(key) {
    try { return localStorage.getItem(key); } catch { return null; }
  }

  function write(key, value) {
    try { localStorage.setItem(key, value); } catch {}
  }

  let theme = read(themeKey);
  if (!neutralThemes.has(theme)) theme = "system";

  function applyTheme() {
    root.dataset.theme = theme === "system" ? (darkMode.matches ? "dark" : "light") : theme;
    document.querySelectorAll("[data-set-theme]").forEach((button) => {
      button.setAttribute("aria-pressed", String(button.dataset.setTheme === theme));
    });
  }

  applyTheme();
  root.classList.add("has-js");
  const savedLanguage = read(languageKey);
  if (savedLanguage === "pt-BR" && root.dataset.restoreLanguage) {
    location.replace(new URL(root.dataset.restoreLanguage + location.search + location.hash, location.href));
  }
  darkMode.addEventListener("change", applyTheme);

  function animate(element, frames, options) {
    if (!reducedMotion.matches && element?.animate) element.animate(frames, options);
  }

  let artwork;
  let artworkMotion = [];

  function updateArtworkControl() {
    const button = artwork?.querySelector("[data-art-control]");
    if (!button) return;
    const playing = artworkMotion[0]?.playState === "running";
    button.textContent = playing ? button.dataset.pause :
      artworkMotion.length ? button.dataset.resume : button.dataset.play;
  }

  function stopArtwork() {
    for (const motion of artworkMotion) motion.cancel();
    artworkMotion = [];
    updateArtworkControl();
  }

  function playArtwork() {
    if (!artwork || reducedMotion.matches || !Element.prototype.animate) return;
    const duration = 4200;
    const needle = artwork.querySelector("[data-art-needle]");
    artworkMotion = [needle.animate([0, 49, 22, 55, 26, 58, 14, 0].map(angle =>
      ({ transform: `rotate(${angle}deg)`, easing: "ease-in-out" })), duration)];

    function pulse(element, starts, intensity) {
      const frames = [{ opacity: 0, offset: 0 }];
      for (const offset of starts) {
        frames.push({ opacity: 0, offset }, { opacity: intensity, offset: offset + .028 },
                    { opacity: 0, offset: offset + .143 });
      }
      frames.push({ opacity: 0, offset: 1 });
      artworkMotion.push(element.animate(frames, duration));
    }

    const beats = [.07, .33, .62];
    for (const [index, offset] of beats.entries()) {
      pulse(artwork.querySelector(`[data-art-key="${index}"]`), [offset], .64);
      pulse(artwork.querySelector(`[data-art-route="${index}"]`), [offset + .046], 1);
    }
    pulse(artwork.querySelector("[data-art-led]"), beats, .9);
    artworkMotion[0].onfinish = stopArtwork;
    updateArtworkControl();
  }

  function initializeArtwork() {
    stopArtwork();
    artwork = document.querySelector("[data-artwork]");
    const button = artwork?.querySelector("[data-art-control]");
    if (!button) return;
    button.hidden = reducedMotion.matches || !Element.prototype.animate;
    updateArtworkControl();
    if (!button.hidden) playArtwork();
  }

  reducedMotion.addEventListener("change", () => {
    if (reducedMotion.matches) stopArtwork();
    const button = artwork?.querySelector("[data-art-control]");
    if (button) button.hidden = reducedMotion.matches || !Element.prototype.animate;
  });

  document.addEventListener("visibilitychange", () => {
    if (document.hidden && artworkMotion[0]?.playState === "running") {
      artworkMotion.forEach(motion => motion.pause());
      updateArtworkControl();
    }
  });

  let observer;
  function initializePage() {
    applyTheme();
    initializeArtwork();
    observer?.disconnect();
    const lift = [{ opacity: .45, transform: "translateY(10px)" }, { opacity: 1, transform: "translateY(0)" }];
    animate(document.querySelector(".hero-copy, .docs"), lift, { duration: 360, easing: "ease-out" });
    animate(document.querySelector(".console"), lift, { duration: 420, easing: "ease-out" });
    if (!("IntersectionObserver" in window) || reducedMotion.matches) return;
    observer = new IntersectionObserver((entries) => {
      for (const { target, isIntersecting } of entries) {
        if (!isIntersecting) continue;
        observer.unobserve(target);
        animate(target, lift, { duration: 350, easing: "ease-out" });
      }
    }, { threshold: .16 });
    document.querySelectorAll("[data-motion]").forEach((section) => observer.observe(section));
  }
  document.addEventListener("DOMContentLoaded", initializePage);
  document.addEventListener("zivi:pagechange", initializePage);

  function galleryTabs(gallery) {
    return [...gallery.querySelectorAll("[data-screen]")];
  }

  function selectedPanel(gallery) {
    return gallery.querySelector(".screen-view:not([hidden])");
  }

  function playStartup(video) {
    if (!video) return;
    if (video.readyState > 0) video.currentTime = 0;
    video.play().catch(() => {});
  }

  function selectScreen(gallery, tab, options = {}) {
    const id = tab.dataset.screen;
    for (const button of galleryTabs(gallery)) {
      const active = tab === button;
      button.setAttribute("aria-selected", String(active));
      button.tabIndex = active ? 0 : -1;
    }
    for (const panel of gallery.querySelectorAll(".screen-view")) {
      panel.hidden = panel.id !== id;
      if (panel.hidden) panel.querySelector("video")?.pause();
    }
    const panel = selectedPanel(gallery);
    const video = panel.querySelector("video");
    const dialog = gallery.querySelector("dialog");
    gallery.querySelector("[data-zoom]").hidden = Boolean(video) || !dialog?.showModal;
    gallery.querySelector("[data-replay]").hidden = !video;
    for (const caption of gallery.querySelectorAll("[data-screen-caption]")) {
      caption.hidden = caption.dataset.screenCaption !== id;
    }
    if (options.focus) tab.focus();
    if (options.announce) gallery.querySelector("[data-screen-status]").textContent = tab.textContent.trim();
    animate(panel, [{ opacity: .64 }, { opacity: 1 }], { duration: 180, easing: "ease-out" });
    if (options.play && !reducedMotion.matches) playStartup(video);
  }

  document.addEventListener("keydown", (event) => {
    const tab = event.target.closest("[data-screen]");
    if (!tab || !["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
    event.preventDefault();
    const gallery = tab.closest("[data-gallery]");
    const tabs = galleryTabs(gallery);
    const delta = event.key === "ArrowLeft" ? -1 : 1;
    let index = (tabs.indexOf(tab) + delta + tabs.length) % tabs.length;
    if (event.key === "Home") index = 0;
    if (event.key === "End") index = tabs.length - 1;
    selectScreen(gallery, tabs[index], { focus: true });
  });

  document.addEventListener("click", (event) => {
    if (event.target.closest("[data-art-control]")) {
      if (!artworkMotion.length) playArtwork();
      else if (artworkMotion[0].playState === "running") artworkMotion.forEach(motion => motion.pause());
      else artworkMotion.forEach(motion => motion.play());
      updateArtworkControl();
      return;
    }
    const themeButton = event.target.closest("[data-set-theme]");
    if (themeButton) {
      theme = themeButton.dataset.setTheme;
      applyTheme();
      write(themeKey, theme);
      return;
    }
    const language = event.target.closest("[data-language]");
    if (language && !event.metaKey && !event.ctrlKey && !event.altKey && !event.shiftKey) {
      write(languageKey, language.dataset.language);
      if (location.hash && language.closest("header") && root.dataset.restoreLanguage && language.dataset.language === "pt-BR") {
        if (["#main-content", "#firmware-screens", "#downloads", "#instrument", "#play"].includes(location.hash)) {
          language.hash = location.hash;
        }
      }
    }
    const gallery = event.target.closest("[data-gallery]");
    if (!gallery) return;
    const tab = event.target.closest("[data-screen]");
    const cycle = event.target.closest("[data-cycle]");
    if (tab) selectScreen(gallery, tab, { play: true });
    if (cycle) {
      const tabs = galleryTabs(gallery);
      const current = tabs.findIndex((button) => button.getAttribute("aria-selected") === "true");
      const next = (current + Number(cycle.dataset.cycle) + tabs.length) % tabs.length;
      selectScreen(gallery, tabs[next], { announce: true });
    }
    if (event.target.closest("[data-replay]")) playStartup(selectedPanel(gallery).querySelector("video"));
    if (event.target.closest("[data-zoom]")) {
      const source = selectedPanel(gallery).querySelector("img");
      const dialog = gallery.querySelector("dialog");
      if (!source || !dialog.showModal) return;
      const image = dialog.querySelector("img");
      image.src = source.currentSrc || source.src;
      image.alt = source.alt;
      dialog.showModal();
    }
  });
})();
