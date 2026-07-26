<script>
  import "@/app.css";
  import { L } from "@lib/store.js";
  import { onMount, onDestroy } from "svelte";

  // Components
  import TopBar from "@comp/TopBar.svelte";
  import ConfigView from "@comp/ConfigView.svelte";
  import ModulesView from "@comp/ModulesView.svelte";
  import LogsView from "@comp/LogsView.svelte";

  // Tabs
  const tabs = {
    config: { component: ConfigView, label: "tabs.config" }, // label 为 locate key
    module: { component: ModulesView, label: "tabs.modules" },
    log: { component: LogsView, label: "tabs.logs" },
  };

  const tabKeys = Object.keys(tabs);

  let activeTab = "config";
  let isSwitching = false;
  // pillIndex drives the sliding pill; it moves *immediately* on tap/drag so
  // the indicator feels responsive even though the page content swaps after
  // the 200ms transition. activeTab still gates which view is mounted.
  let pillIndex = tabKeys.indexOf(activeTab);

  function switchTab(key) {
    const idx = tabKeys.indexOf(key);
    pillIndex = idx; // slide the pill right away
    if (activeTab === key || isSwitching) return;
    isSwitching = true;
    setTimeout(() => {
      activeTab = key;
      isSwitching = false;
    }, 200);
  }

  // Drag the pill between tabs, like the KSU-Next bar.
  let barEl;
  let dragging = false;

  function indexFromClientX(clientX) {
    if (!barEl) return pillIndex;
    const pad = 7;
    const rect = barEl.getBoundingClientRect();
    const inner = rect.width - pad * 2;
    const x = Math.min(Math.max(clientX - rect.left - pad, 0), inner - 1);
    return Math.min(Math.floor((x / inner) * tabKeys.length), tabKeys.length - 1);
  }

  function onBarPointerDown(e) {
    // only start a drag if the press lands on the current pill
    if (indexFromClientX(e.clientX) !== pillIndex) return;
    dragging = true;
    try {
      barEl.setPointerCapture(e.pointerId);
    } catch (_) {}
  }

  function onBarPointerMove(e) {
    if (!dragging) return;
    pillIndex = indexFromClientX(e.clientX);
  }

  function onBarPointerUp() {
    if (!dragging) return;
    dragging = false;
    switchTab(tabKeys[pillIndex]);
  }

  // Sheen: a soft highlight tracked to pointer/gyro position, same effect as
  // the KernelNoSU module WebUI. Cleaned up on destroy since this is the
  // root component and listeners would otherwise leak across HMR/navigation.
  let cleanupSheen = () => {};

  onMount(() => {
    if (matchMedia("(prefers-reduced-motion: reduce)").matches) return;

    let scrolledAt = 0;
    let raf = 0;
    let gx = 50;
    let gy = 15;

    const onScroll = () => {
      scrolledAt = Date.now();
    };

    function paint() {
      raf = 0;
      document.querySelectorAll(".card").forEach((el) => {
        el.style.setProperty("--gx", gx.toFixed(1) + "%");
        el.style.setProperty("--gy", gy.toFixed(1) + "%");
      });
    }

    function set(px, py) {
      if (Date.now() - scrolledAt < 180) return;
      gx = 50 + px * 32;
      gy = 22 + py * 32;
      document.body.classList.add("sheen");
      if (!raf) raf = requestAnimationFrame(paint);
    }

    function onOrient(e) {
      if (e.gamma == null && e.beta == null) return;
      set(
        Math.max(-25, Math.min(25, e.gamma || 0)) / 25,
        Math.max(-40, Math.min(40, (e.beta || 0) - 40)) / 40,
      );
    }

    function gyro() {
      if (typeof DeviceOrientationEvent === "undefined") return;
      if (typeof DeviceOrientationEvent.requestPermission === "function") {
        DeviceOrientationEvent.requestPermission()
          .then((s) => {
            if (s === "granted") addEventListener("deviceorientation", onOrient);
          })
          .catch(() => {});
      } else {
        addEventListener("deviceorientation", onOrient);
      }
    }

    function onPointerMove(ev) {
      if (ev.pointerType === "touch") return;
      set((ev.clientX / innerWidth) * 2 - 1, (ev.clientY / innerHeight) * 2 - 1);
    }

    gyro();
    addEventListener("touchstart", gyro, { once: true, passive: true });
    addEventListener("scroll", onScroll, { passive: true });
    addEventListener("pointermove", onPointerMove, { passive: true });

    cleanupSheen = () => {
      if (raf) cancelAnimationFrame(raf);
      removeEventListener("deviceorientation", onOrient);
      removeEventListener("touchstart", gyro);
      removeEventListener("scroll", onScroll);
      removeEventListener("pointermove", onPointerMove);
      document.body.classList.remove("sheen");
    };
  });

  onDestroy(() => cleanupSheen());
</script>

<div class="app-root">
  <TopBar />

  <div class="app-main">
    {#if isSwitching}
      <div class="tab-overlay"><div class="tab-spinner"></div></div>
    {/if}

    {#key activeTab}
      <svelte:component this={tabs[activeTab].component} />
    {/key}
  </div>

  <div
    class="bottom-bar {dragging ? 'dragging' : ''}"
    style="--n:{tabKeys.length}; --i:{pillIndex}"
    bind:this={barEl}
    on:pointerdown={onBarPointerDown}
    on:pointermove={onBarPointerMove}
    on:pointerup={onBarPointerUp}
    on:pointercancel={onBarPointerUp}
  >
    <div class="tab-pill"></div>
    {#each tabKeys as key, i}
      <button
        type="button"
        class="tab-btn {pillIndex === i ? 'active' : ''}"
        on:click={() => switchTab(key)}
      >
        {$L[tabs[key].label.split(".")[0]][tabs[key].label.split(".")[1]]}
      </button>
    {/each}
  </div>
</div>
