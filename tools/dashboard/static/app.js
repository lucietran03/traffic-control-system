/* Read-only live dashboard client. Polls /state.json (served by
 * server.py, which tails c_main's stdout) every 500ms and redraws.
 * No WebSocket, no build step, no dependency - plain DOM/SVG. */

const PHASE_NAMES = [
  "ARTERIAL GREEN", "ARTERIAL YELLOW", "ALL-RED (A→B)",
  "CONNECTOR GREEN", "CONNECTOR YELLOW", "ALL-RED (B→A)",
];
const MODE_NAMES = ["PEAK_FIXED", "OFF_PEAK_SENSOR"];
const SUPERVISORY_NAMES = ["FAULT_SAFE", "RAILWAY_PREEMPTION", "CENTRAL_OVERRIDE", "NORMAL_OPERATION"];
const CROSSING_NAMES = ["OPEN", "WARNING", "CLOSED", "FAULT"];

const FAULT_BITS = [
  [1 << 0, "GATE_CONFIRM_MISSING"],
  [1 << 1, "TRAIN_SENSOR_STUCK"],
  [1 << 2, "PED_BUTTON_STUCK"],
  [1 << 3, "VEHICLE_SENSOR_STUCK"],
  [1 << 4, "WATCHDOG_TRIP"],
];
const SENSOR_BITS = [
  [1 << 0, "ARTERIAL_DEMAND"],
  [1 << 1, "CONNECTOR_DEMAND"],
  [1 << 2, "PED_LATCHED_SIDE_0"],
  [1 << 3, "PED_LATCHED_SIDE_1"],
  [1 << 4, "PED_LATCHED_SIDE_2"],
  [1 << 5, "PED_LATCHED_SIDE_3"],
  [1 << 6, "QUEUE_WARNING"],
];

// Static topology layout (matches root README's diagram + RL<->Lx adjacency
// from app/railway/src/rlx_comm.c's ADJACENCY table: RL1<->{L1,L2},
// RL2<->{L3,L4}, RL3<->{L5,L6}).
const LX_IDS = ["L1", "L2", "L3", "L4", "L5", "L6"];
const RLX_IDS = ["RL1", "RL2", "RL3"];
const ADJACENCY = { RL1: ["L1", "L2"], RL2: ["L3", "L4"], RL3: ["L5", "L6"] };

const LAYOUT = {};
LAYOUT["C1"] = { x: 450, y: 50, w: 90, h: 50 };
LX_IDS.forEach((id, i) => {
  LAYOUT[id] = { x: 60 + i * 135, y: 180, w: 100, h: 55 };
});
RLX_IDS.forEach((id, i) => {
  const [a, b] = ADJACENCY[id];
  const x = (LAYOUT[a].x + LAYOUT[b].x) / 2 + LAYOUT[a].w / 2 - 45;
  LAYOUT[id] = { x, y: 330, w: 90, h: 55 };
});

let selectedId = null;
let overviewBuilt = false;

function svgEl(tag, attrs) {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const k in attrs) el.setAttribute(k, attrs[k]);
  return el;
}

function buildOverviewOnce() {
  const svg = document.getElementById("overview-svg");
  svg.innerHTML = "";

  // Connecting lines first (drawn under the boxes): C1 to every node, RLx to its 2 adjacent Lx.
  const allIds = ["C1", ...LX_IDS, ...RLX_IDS];
  for (const id of allIds) {
    if (id === "C1") continue;
    const a = LAYOUT["C1"], b = LAYOUT[id];
    svg.appendChild(svgEl("line", {
      class: "link-line",
      x1: a.x + a.w / 2, y1: a.y + a.h,
      x2: b.x + b.w / 2, y2: b.y,
    }));
  }
  for (const rlx of RLX_IDS) {
    for (const lx of ADJACENCY[rlx]) {
      const a = LAYOUT[rlx], b = LAYOUT[lx];
      svg.appendChild(svgEl("line", {
        class: "link-line",
        x1: a.x + a.w / 2, y1: a.y,
        x2: b.x + b.w / 2, y2: b.y + b.h,
      }));
    }
  }

  for (const id of allIds) {
    const pos = LAYOUT[id];
    const g = svgEl("g", { "data-id": id });

    const rect = svgEl("rect", {
      class: "node-box state-grey",
      id: `box-${id}`,
      x: pos.x, y: pos.y, width: pos.w, height: pos.h, rx: 10,
    });
    g.appendChild(rect);

    const label = svgEl("text", { class: "node-label", x: pos.x + pos.w / 2, y: pos.y + pos.h / 2 - 2 });
    label.textContent = id;
    g.appendChild(label);

    const sub = svgEl("text", { class: "node-sub", id: `sub-${id}`, x: pos.x + pos.w / 2, y: pos.y + pos.h / 2 + 14 });
    sub.textContent = id === "C1" ? "central" : "";
    g.appendChild(sub);

    if (id !== "C1") {
      g.style.cursor = "pointer";
      g.addEventListener("click", () => selectNode(id));
    }
    svg.appendChild(g);
  }
  overviewBuilt = true;
}

function colorClassFor(node) {
  if (!node || node.availability === "UNAVAILABLE") return "state-grey";
  if (node.role === "RAILWAY") {
    switch (node.crossing_state) {
      case 0: return "state-green";  // OPEN
      case 1: return "state-amber";  // WARNING
      case 2: return "state-amber";  // CLOSED (protecting a train - active, not a fault)
      case 3: return "state-red";    // FAULT
      default: return "state-grey";
    }
  }
  if (node.role === "INTERSECTION") {
    switch (node.supervisory) {
      case 0: return "state-red";    // FAULT_SAFE
      case 1: return "state-amber";  // RAILWAY_PREEMPTION
      case 2: return "state-purple"; // CENTRAL_OVERRIDE
      case 3: return "state-green";  // NORMAL_OPERATION
      default: return "state-grey";
    }
  }
  return "state-grey";
}

function subLabelFor(node) {
  if (!node) return "no data yet";
  if (node.availability === "UNAVAILABLE") return "UNAVAILABLE";
  if (node.role === "RAILWAY") return CROSSING_NAMES[node.crossing_state] ?? "?";
  if (node.role === "INTERSECTION") return SUPERVISORY_NAMES[node.supervisory] ?? "?";
  return "";
}

function updateOverview(state) {
  if (!overviewBuilt) buildOverviewOnce();
  const nodes = state.nodes || {};
  const c1Fresh = state.last_update_ts && (Date.now() / 1000 - state.last_update_ts) < 5;
  document.getElementById("box-C1").setAttribute("class", `node-box ${c1Fresh ? "state-green" : "state-grey"}`);

  for (const id of [...LX_IDS, ...RLX_IDS]) {
    const node = nodes[id];
    const box = document.getElementById(`box-${id}`);
    const cls = colorClassFor(node) + (id === selectedId ? " selected" : "");
    box.setAttribute("class", `node-box ${cls}`);
    document.getElementById(`sub-${id}`).textContent = subLabelFor(node);
  }
}

function decodeBits(value, table) {
  if (value === null || value === undefined) return [];
  const out = [];
  for (const [bit, name] of table) {
    if (value & bit) out.push(name);
  }
  return out;
}

function selectNode(id) {
  selectedId = id;
  document.getElementById("detail-empty").classList.add("hidden");
  document.getElementById("detail-content").classList.remove("hidden");
  render(); // refresh immediately with current cached state
}

function renderDetail(state) {
  if (!selectedId) return;
  const node = (state.nodes || {})[selectedId];
  document.getElementById("detail-title").textContent = selectedId +
    (node ? ` — ${node.role}` : " — no data yet");

  drawDetailDiagram(selectedId, node);

  const table = document.getElementById("detail-table");
  table.innerHTML = "";
  const rows = [];
  if (!node) {
    rows.push(["Status", "No data received yet"]);
  } else {
    rows.push(["Availability", node.availability]);
    if (node.role === "INTERSECTION") {
      rows.push(["Mode", MODE_NAMES[node.mode] ?? node.mode]);
      rows.push(["Phase", PHASE_NAMES[node.phase] ?? node.phase]);
      rows.push(["Supervisory", SUPERVISORY_NAMES[node.supervisory] ?? node.supervisory]);
      rows.push(["Override active", node.override_active ? "YES" : "no"]);
    } else if (node.role === "RAILWAY") {
      rows.push(["Crossing state", CROSSING_NAMES[node.crossing_state] ?? node.crossing_state]);
    }
  }
  for (const [k, v] of rows) {
    const tr = document.createElement("tr");
    const td1 = document.createElement("td"); td1.textContent = k;
    const td2 = document.createElement("td"); td2.textContent = v;
    tr.append(td1, td2);
    table.appendChild(tr);
  }

  const faultsEl = document.getElementById("detail-faults");
  const sensorsEl = document.getElementById("detail-sensors");
  if (!node) {
    faultsEl.innerHTML = ""; sensorsEl.innerHTML = "";
  } else {
    const faults = decodeBits(node.faults, FAULT_BITS);
    faultsEl.innerHTML = "<strong>Faults:</strong> " + (faults.length
      ? faults.map(f => `<span class="badge badge-fault">${f}</span>`).join("")
      : '<span class="badge badge-none">none</span>');
    if (node.role === "INTERSECTION") {
      const sensors = decodeBits(node.sensor_status, SENSOR_BITS);
      sensorsEl.innerHTML = "<br><strong>Sensors:</strong> " + (sensors.length
        ? sensors.map(s => `<span class="badge badge-sensor">${s}</span>`).join("")
        : '<span class="badge badge-none">none</span>');
    } else {
      sensorsEl.innerHTML = "";
    }
  }
}

function drawDetailDiagram(id, node) {
  const svg = document.getElementById("detail-svg");
  svg.innerHTML = "";
  const isRailway = id.startsWith("RL");

  if (isRailway) {
    // Simple crossing: horizontal track, vertical road, two gate arms.
    const crossing = node ? node.crossing_state : null;
    const gateColor = crossing === 0 ? "#22c55e" : (crossing === 3 ? "#ef4444" : "#f59e0b");
    svg.appendChild(svgEl("rect", { x: 0, y: 110, width: 260, height: 12, fill: "#3b4257" })); // track
    svg.appendChild(svgEl("rect", { x: 120, y: 0, width: 20, height: 260, fill: "#232c42" })); // road
    // gate arms (rotate toward "down" when not OPEN)
    const downAngle = (crossing === 0) ? 0 : 80;
    [ [70, 116, 1], [190, 116, -1] ].forEach(([cx, cy, dir]) => {
      const g = svgEl("g", { transform: `translate(${cx},${cy}) rotate(${dir * downAngle})` });
      g.appendChild(svgEl("rect", { x: 0, y: -3, width: 45 * dir, height: 6, fill: gateColor, rx: 2 }));
      svg.appendChild(g);
    });
    svg.appendChild(svgEl("circle", { cx: 130, cy: 40, r: 8, fill: gateColor }));
  } else {
    // Simple intersection: two crossing roads + one signal head per approach.
    svg.appendChild(svgEl("rect", { x: 0, y: 110, width: 260, height: 40, fill: "#232c42" })); // arterial (E-W)
    svg.appendChild(svgEl("rect", { x: 110, y: 0, width: 40, height: 260, fill: "#232c42" })); // connector (N-S)

    const phase = node ? node.phase : null;
    const arterialColor = [0, 1].includes(phase) ? (phase === 0 ? "#22c55e" : "#f59e0b") : "#ef4444";
    const connectorColor = [3, 4].includes(phase) ? (phase === 3 ? "#22c55e" : "#f59e0b") : "#ef4444";

    svg.appendChild(svgEl("circle", { cx: 30, cy: 130, r: 10, fill: arterialColor }));
    svg.appendChild(svgEl("circle", { cx: 230, cy: 130, r: 10, fill: arterialColor }));
    svg.appendChild(svgEl("circle", { cx: 130, cy: 30, r: 10, fill: connectorColor }));
    svg.appendChild(svgEl("circle", { cx: 130, cy: 230, r: 10, fill: connectorColor }));

    if (node && node.sensor_status !== null) {
      const pedSides = [2, 3, 4, 5].map(bit => !!(node.sensor_status & (1 << bit)));
      const pedPos = [[40, 150], [220, 150], [150, 40], [150, 220]];
      pedSides.forEach((active, i) => {
        if (active) svg.appendChild(svgEl("rect", {
          x: pedPos[i][0] - 5, y: pedPos[i][1] - 5, width: 10, height: 10,
          fill: "#7dd3fc", rx: 2,
        }));
      });
    }
  }
}

let lastState = null;
function render() {
  if (lastState) {
    updateOverview(lastState);
    renderDetail(lastState);
  }
}

async function poll() {
  try {
    const res = await fetch("/state.json", { cache: "no-store" });
    const state = await res.json();
    lastState = state;
    render();

    const statusEl = document.getElementById("conn-status");
    const ageSec = Date.now() / 1000 - (state.last_update_ts || 0);
    if (!state.last_update_ts) {
      statusEl.textContent = "waiting for c_main data…";
      statusEl.className = "conn-status conn-unknown";
    } else if (ageSec < 5) {
      statusEl.textContent = "LIVE";
      statusEl.className = "conn-status conn-live";
    } else {
      statusEl.textContent = `stale (${Math.round(ageSec)}s since last update)`;
      statusEl.className = "conn-status conn-stale";
    }
  } catch (e) {
    const statusEl = document.getElementById("conn-status");
    statusEl.textContent = "dashboard server unreachable";
    statusEl.className = "conn-status conn-stale";
  }
}

buildOverviewOnce();
poll();
setInterval(poll, 1000);
