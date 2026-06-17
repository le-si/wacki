(function () {
  "use strict";

  // Platform metadata is injected by Hugo (partials/platforms-data.html) from
  // the content/platformy/*.md single source. Labels/assets come from there;
  // only the UA sniff stays in JS, because order matters (Android UA also
  // contains "linux", so it must be checked first — data weight order can't
  // guarantee that).
  var PLATFORMS = window.WACKI_PLATFORMS || [];

  function platformByKey(key) {
    for (var i = 0; i < PLATFORMS.length; i++) {
      if (PLATFORMS[i].key === key) return PLATFORMS[i];
    }
    return null;
  }

  // Returns a platform key matching content/platformy/<key>.md (and the card's
  // data-platform attribute), or null on iOS / unknown.
  function detectPlatform() {
    var ua = navigator.userAgent || "";
    var p = (navigator.userAgentData && navigator.userAgentData.platform) ||
            navigator.platform || "";
    var s = (ua + " " + p).toLowerCase();

    if (/android/.test(s)) return "android";
    if (/iphone|ipad|ipod/.test(s)) return null;
    if (/win/.test(s)) return "windows";
    if (/mac/.test(s)) return "macos";
    if (/linux|x11|cros/.test(s)) return "linux";
    return null;
  }

  var platform = detectPlatform();

  if (platform) {
    var card = document.querySelector('.card[data-platform="' + platform + '"]');
    if (card) card.classList.add("card--recommended");

    // Hero CTA becomes a 1-click direct download for the detected OS. The grid
    // tiles route to the per-platform pages; the hero is the express lane.
    var meta = platformByKey(platform);
    var heroBtn = document.getElementById("hero-download");
    if (heroBtn && meta) {
      heroBtn.textContent = "⬇ Pobierz dla " + meta.label;
      if (meta.dl) {
        heroBtn.href = meta.dl;
        heroBtn.setAttribute("data-umami-event", "download");
        heroBtn.setAttribute("data-umami-event-platform", meta.key);
      }
    }
  }

  var burger = document.querySelector(".nav__burger");
  var links = document.querySelector(".nav__links");
  if (burger && links) {
    burger.addEventListener("click", function () {
      var open = links.classList.toggle("open");
      burger.setAttribute("aria-expanded", open ? "true" : "false");
    });
    links.addEventListener("click", function (e) {
      if (e.target.tagName === "A") {
        links.classList.remove("open");
        burger.setAttribute("aria-expanded", "false");
      }
    });
  }

  var toTop = document.querySelector(".totop");
  if (toTop) {
    var onScroll = function () {
      if (window.scrollY > 600) toTop.classList.add("show");
      else toTop.classList.remove("show");
    };
    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();
  }
})();

(function () {
  "use strict";
  if (!window.matchMedia || !window.matchMedia("(hover: hover) and (pointer: fine)").matches) return;

  var img = new Image();
  img.onload = init;
  img.src = window.WACKI_CURSOR_SHEET || "assets/cursor-pencil-sheet.png";

  function init() {
    var cur = document.createElement("div");
    cur.className = "cursor";
    cur.setAttribute("aria-hidden", "true");
    document.body.appendChild(cur);
    document.body.classList.add("has-cursor");

    var INTERACTIVE = "a, button, summary, .btn, .card, label, [role='button']";
    var shown = false;

    window.addEventListener("pointermove", function (e) {
      if (e.pointerType && e.pointerType !== "mouse") return;
      cur.style.transform = "translate(" + e.clientX + "px," + e.clientY + "px)";
      if (!shown) { cur.style.display = "block"; shown = true; }
      var hot = e.target && e.target.closest && e.target.closest(INTERACTIVE);
      cur.classList.toggle("is-anim", !!hot);
    }, { passive: true });

    document.addEventListener("mouseleave", hide);
    window.addEventListener("blur", hide);
    function hide() { cur.style.display = "none"; shown = false; }
  }
})();
