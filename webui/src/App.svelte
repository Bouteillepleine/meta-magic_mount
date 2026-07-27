<script>
  import "@/app.css";
  import { L } from "@lib/store.js";

  // Components
  import TopBar from "@comp/TopBar.svelte";
  import ConfigView from "@comp/ConfigView.svelte";
  import ModulesView from "@comp/ModulesView.svelte";

  // Tabs
  const tabs = {
    config: { component: ConfigView, label: "tabs.config" }, // label 为 locate key
    module: { component: ModulesView, label: "tabs.modules" },
  };

  // MD3 icons for the nav (config=tune, module=extension)
  const ICONS = {
    config:
      "M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z",
    module:
      "M20.5 11H19V7c0-1.1-.9-2-2-2h-4V3.5C13 2.12 11.88 1 10.5 1S8 2.12 8 3.5V5H4c-1.1 0-1.99.9-1.99 2v3.8H3.5c1.49 0 2.7 1.21 2.7 2.7s-1.21 2.7-2.7 2.7H2V22c0 1.1.9 2 2 2h3.8v-1.5c0-1.49 1.21-2.7 2.7-2.7 1.49 0 2.7 1.21 2.7 2.7V24H17c1.1 0 2-.9 2-2v-4h1.5c1.38 0 2.5-1.12 2.5-2.5S21.88 11 20.5 11z",
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
    const pad = 8;
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
        <span class="ic"><svg viewBox="0 0 24 24"><path d={ICONS[key]} /></svg></span>
        <span>{$L[tabs[key].label.split(".")[0]][tabs[key].label.split(".")[1]]}</span>
      </button>
    {/each}
  </div>
</div>
