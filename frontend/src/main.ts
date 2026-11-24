import "./style.css";

const groups: [string, string, string[], string[], number, boolean?][] = [
  ["Solar power", "W", ["PPV", "H21", "H23"], ["Panel", "Peak today", "Peak yesterday"], 1],
  ["Voltage", "V", ["V", "VPV"], ["Battery", "Panel"], .001],
  ["Current", "A", ["I", "IL"], ["Battery", "Load"], .001],
  ["Daily solar yield", "kWh", ["H20", "H22"], ["Today", "Yesterday"], .01],
  ["Total solar yield", "kWh", ["H19"], ["Lifetime counter"], .01],
  ["Temperature", "C", ["T1", "T2", "T3"], ["Sensor 1", "Sensor 2", "Sensor 3"], 1],
  ["Charger state", "code", ["CS", "MPPT", "LOAD"], ["Charge stage", "MPPT mode", "Load output"], 1, true],
  ["Charger errors", "code", ["ERR"], ["Error"], 1, true],
];
const colors = ["#f8c462", "#65dfbd", "#a9bafe"];
const form = document.querySelector("form")!;
const date = document.querySelector("input")!;
const status = document.querySelector("#status")!;
const charts = document.querySelector("#charts")!;
const localDate = new Date(Date.now() - new Date().getTimezoneOffset() * 60000);
date.value = localDate.toISOString().slice(0, 10);

type Sample = { minute: number; values: Record<string, number> };

function parse(text: string, day: string): Sample[] {
  const [header, ...lines] = text.trimEnd().split(/\r?\n/);
  const fields = header.split(",");
  return lines.flatMap(line => {
    const columns = line.split(",");
    // Older firmware used an RTC timestamp with an embedded weekday comma.
    const oldStamp = /^[A-Za-z]{3}$/.test(columns[0]) && columns.length >= 5;
    const stamp = oldStamp ? columns.slice(0, 2).join(",") : columns[0];
    const cells = columns.slice(oldStamp ? 2 : 1);
    const sampleDay = oldStamp
      ? stamp.replace(/^[A-Za-z]{3}, ([A-Za-z]{3}) (\d\d) (\d{4}).*$/, (_, month, day, year) => {
          const monthIndex = "Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec".split(" ").indexOf(month);
          return year + "-" + String(monthIndex + 1).padStart(2, "0") + "-" + day;
        })
      : stamp;
    if (stamp.length > 8 && !sampleDay.startsWith(day)) return [];
    const time = /(\d\d):(\d\d):(\d\d)$/.exec(stamp);
    if (!time) return [];
    const minute = +time[1] * 60 + +time[2] + +time[3] / 60;
    const values: Record<string, number> = {};
    cells.forEach((cell, index) => {
      const field = fields[index + 1];
      const key = ({ temp1: "T1", temp2: "T2", temp3: "T3", Temperature: "T1" } as Record<string, string>)[field] ?? field;
      if (key && cell.trim() && Number.isFinite(+cell) && !(key.startsWith("T") && +cell === -127))
        values[key] = +cell;
    });
    return [{ minute, values }];
  }).sort((a, b) => a.minute - b.minute);
}

function draw(group: typeof groups[number], samples: Sample[]) {
  const [title, unit, fields, labels, scale, stepped] = group;
  const section = document.createElement("section");
  const heading = document.createElement("h2");
  heading.textContent = title;
  const legend = document.createElement("p");
  legend.className = "legend";
  section.append(heading, legend);
  charts.append(section);
  const points = fields.map(field => samples.map(sample => Number.isFinite(sample.values[field]) ? sample.values[field] * scale : NaN));
  const values = points.flat().filter(Number.isFinite);
  if (!values.length) {
    legend.textContent = "No readings for this date";
    return;
  }
  const canvas = document.createElement("canvas");
  canvas.setAttribute("aria-label", title + " by time of day");
  section.append(canvas);
  const ctx = canvas.getContext("2d")!;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const padding = max === min ? (stepped ? 1 : Math.max(.1, Math.abs(max) * .03)) : (max - min) * .08;
  function show(index: number) {
    legend.replaceChildren();
    fields.forEach((_, series) => {
      const item = document.createElement("span");
      item.style.color = colors[series];
      const value = points[series][index];
      item.textContent = labels[series] + " " + (Number.isFinite(value) ? value.toLocaleString(undefined, { maximumFractionDigits: 3 }) + " " + unit : "--");
      legend.append(item);
    });
  }
  const render = () => {
    if (!canvas.isConnected) return;
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    const ratio = window.devicePixelRatio || 1;
    canvas.width = width * ratio;
    canvas.height = height * ratio;
    ctx.scale(ratio, ratio);
    const left = 46, top = 10, bottom = height - 24, right = width - 16;
    const x = (minute: number) => left + minute / 1440 * (right - left);
    const y = (value: number) => bottom - (value - min + padding) / (max - min + 2 * padding) * (bottom - top);
    ctx.font = "11px system-ui";
    ctx.fillStyle = "#a3b2c2";
    ctx.strokeStyle = "#324153";
    for (let tick = 0; tick <= 4; tick++) {
      const level = min - padding + (max - min + 2 * padding) * tick / 4;
      ctx.fillText(Number(level.toPrecision(3)).toString(), 0, y(level) + 4);
      ctx.beginPath(); ctx.moveTo(left, y(level)); ctx.lineTo(right, y(level)); ctx.stroke();
      ctx.fillText(String(tick * 6).padStart(2, "0") + ":00", x(tick * 360) - 15, height - 5);
    }
    points.forEach((series, line) => {
      ctx.strokeStyle = ctx.fillStyle = colors[line];
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      let previous = -1;
      series.forEach((value, index) => {
        if (!Number.isFinite(value)) { previous = -1; return; }
        const at = x(samples[index].minute);
        if (previous < 0 || samples[index].minute - samples[previous].minute > 21) ctx.moveTo(at, y(value));
        else {
          if (stepped) ctx.lineTo(at, y(series[previous]));
          ctx.lineTo(at, y(value));
        }
        ctx.fillRect(at - 1, y(value) - 1, 2, 2);
        previous = index;
      });
      ctx.stroke();
    });
  };
  observer.observe(canvas);
  redraw.set(canvas, render);
  canvas.onpointermove = event => {
    const minute = (event.offsetX - 46) / (canvas.clientWidth - 62) * 1440;
    const nearest = samples.reduce((best, sample, index) => Math.abs(sample.minute - minute) < Math.abs(samples[best].minute - minute) ? index : best, 0);
    show(nearest);
    const time = samples[nearest].minute;
    legend.append("  " + String(Math.floor(time / 60)).padStart(2, "0") + ":" + String(Math.floor(time % 60)).padStart(2, "0"));
  };
  canvas.onpointerleave = () => show(samples.length - 1);
  show(samples.length - 1);
}

const redraw = new Map<Element, () => void>();
const observer = new ResizeObserver(entries => entries.forEach(entry => redraw.get(entry.target)?.()));
let controller: AbortController | undefined;
let legacy: { month: string; text: string } | undefined;

async function load() {
  const day = date.value;
  if (!day) return;
  controller?.abort();
  const request = controller = new AbortController();
  observer.disconnect();
  redraw.clear();
  charts.replaceChildren();
  status.textContent = "Loading " + day + "...";
  try {
    const options = { signal: request.signal };
    const response = await fetch("./data/" + day + ".csv", options);
    let text = "";
    if (response.ok) text = await response.text();
    else if (response.status === 404) {
      const month = day.slice(0, 7);
      if (legacy?.month === month) text = legacy.text;
      else {
        const monthly = await fetch("./data/" + month + ".csv", options);
        if (monthly.ok) {
          text = await monthly.text();
          legacy = { month, text };
        } else if (monthly.status !== 404) throw new Error(String(monthly.status));
      }
    } else throw new Error(String(response.status));
    if (request.signal.aborted) return;
    const samples = parse(text, day);
    status.textContent = samples.length ? day + " / " + samples.length + " samples" : "No samples for " + day;
    if (samples.length) groups.forEach(group => draw(group, samples));
  } catch {
    if (!request.signal.aborted) status.textContent = "Could not read the log for " + day + ".";
  }
}

form.onsubmit = event => { event.preventDefault(); void load(); };
const epoch = String(Math.floor(localDate.getTime() / 1000));
void fetch("./api/setTime", { method: "POST", body: new URLSearchParams({ epoch }) })
  .catch(() => undefined).then(load);
