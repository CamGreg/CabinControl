import "./style.css";
import Chart from "chart.js/auto";
import zoomPlugin from "chartjs-plugin-zoom";
import "chartjs-adapter-date-fns";

Chart.register(zoomPlugin);
Chart.defaults.color = "whitesmoke";
Chart.defaults.borderColor = "gray";
Chart.defaults.backgroundColor = "whitesmoke";

// Placeholder for battery percentage update logic
function updateBatteryPercentage(percentage: number) {
  const batteryPercentageElement =
    document.getElementById("battery-percentage");
  if (batteryPercentageElement) {
    batteryPercentageElement.innerText = percentage + "%";
  }

  const batteryLevelGroup = document.getElementById("battery-level");
  if (batteryLevelGroup) {
    const rectangles = batteryLevelGroup.getElementsByTagName("rect");
    const numRectangles = rectangles.length;
    const filledRectangles = Math.round(percentage / (100 / numRectangles));

    for (let i = 0; i < numRectangles; i++) {
      if (i < filledRectangles) {
        rectangles[i].setAttribute("width", "8");
      } else {
        rectangles[i].setAttribute("width", "0");
      }
    }
  }
}
type myDataSet = {
  label: string;
  backgroundColor: string;
  borderColor: string;
  data: any[];
  yAxisID: string;
};

document.addEventListener("DOMContentLoaded", () => {
  const currentMonthDisplay = document.getElementById("current-month-display");

  const chartCanvas = document.getElementById("myChart") as HTMLCanvasElement;
  if (chartCanvas === null) {
    console.error("Chart canvas not found!");
    return;
  }
  let myChart: Chart; // To store chart instance for later updates/destruction
  async function fetchCsvData(): Promise<string | null> {
    const now = new Date();
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, "0"); // Month is 0-indexed
    const filename = `${year}-${month}.csv`;
    const csvUrl = `/data/${filename}`; // Matches your ESP32 endpoint
    if (currentMonthDisplay != null) {
      currentMonthDisplay.textContent = `${year}-${month}`;
    }
    console.log(`Fetching CSV from: ${csvUrl}`);
    try {
      const response = await fetch(csvUrl);
      if (!response.ok) {
        if (response.status === 404) {
          throw new Error(`Log file not found for ${filename}.`);
        }
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      const csvText = await response.text();
      console.log("CSV data fetched successfully.");
      return csvText;
    } catch (error) {
      console.error("Could not fetch CSV data:", error);
      alert(
        `Failed to load data: ${error}. Please ensure the ESP32 is running and the file exists.`,
      );
      return null;
    }
  }

  function InitChart() {
    if (myChart) {
      myChart.destroy(); // Destroy previous chart instance if it exists
    }
    if (chartCanvas === null) {
      console.error("Chart canvas not found!");
      return;
    }
    const config = {
      type: "line",
      data: {
        datasets: [] as myDataSet[],
      },
      options: {
        // parsing: false,
        spanGaps: true,
        animation: false,
        responsive: true,
        datasets: {
          line: {
            pointRadius: 0, // disable for all `'line'` datasets
          },
        },
        stacked: false,
        plugins: {
          legend: {
            position: "bottom",
            // eslint-disable-next-line @typescript-eslint/ban-ts-comment
            // @ts-ignore
            onClick: function (event: any, legendItem: any, legend: any) {
              // eslint-disable-next-line @typescript-eslint/ban-ts-comment
              // @ts-ignore
              const ci = this.chart;

              ci.data.datasets.forEach(function (e: any, i: any) {
                var meta = ci.getDatasetMeta(i);

                if (i === legendItem.datasetIndex) {
                  meta.hidden = !meta.hidden;
                  // eslint-disable-next-line @typescript-eslint/ban-ts-comment
                  // @ts-ignore
                  myChart.config.options.scales[e.yAxisID].display =
                    !meta.hidden;
                }
              });

              ci.update();
            },
          },
          // title: {
          //     display: false,
          //     text: 'Industrial Plankton Log Viewer'
          // },
          zoom: {
            limits: {
              // y: { min: 'original', max: 'original' },
              y: { min: 0 },
            },
            pan: {
              enabled: true,
              mode: "xy",
              scaleMode: "xy",
            },
            zoom: {
              wheel: {
                enabled: true,
              },
              pinch: {
                enabled: true,
              },
              mode: "xy",
              scaleMode: "xy",
            },
          },
        },
        scales: {
          x: {
            display: true,
            position: "bottom",
            type: "time",
            ticks: {
              source: "auto",
              autoSkip: true,
              autoSkipPadding: 50,
              maxRotation: 0,
              major: {
                enabled: true,
              },
            },
            time: {
              displayFormats: {
                hour: "HH:mm",
                minute: "HH:mm",
                second: "HH:mm:ss",
              },
            },
          },
        },
      },
    };

    myChart = new Chart(chartCanvas, config as any);
  }
  // Initial data load and chart render
  fetchCsvData().then((data) => {
    if (data != null) {
      InitChart();
      ParseLogs(data, myChart);
    } else {
      console.warn("No data to render chart.");
    }
  });
  // Optional: Auto-reload data periodically if logs are updated frequently
  // setInterval(() => {
  //     fetchCsvData().then(data => {
  //         if (data && data.labels.length > 0) {
  //             renderChart(data);
  //         }
  //     });
  // }, 60000); // Reload every minute
});

// Example:  Assuming you have a function to fetch the battery percentage
async function getBatteryPercentage(): Promise<number> {
  return Math.floor(Math.random() * 100); // Mock data
}

async function updateBatteryDisplay() {
  const batteryPercentage = await getBatteryPercentage();
  updateBatteryPercentage(batteryPercentage);
}

// Call updateBatteryDisplay initially and then periodically
updateBatteryDisplay();
setInterval(updateBatteryDisplay, 5000); // Update every 5 seconds

function ParseLogs(rawCSV: string, chart: Chart) {
  let dataLines = rawCSV
    .split("\n")
    .filter((line) => line.length > 0)
    .filter((line) => !line.startsWith("Event:"))
    .filter((line) => !line.startsWith("Culture"))
    .filter((line) => !line.startsWith("IP"))
    .map((line) => line.replaceAll("\t", ","));

  const HeaderLineString = dataLines.shift();
  if (HeaderLineString == null) {
    console.error("Header line not found!");
    return;
  }
  const HeaderLine = HeaderLineString.trim().toLowerCase().split(",");

  let data = dataLines
    .filter((line) => {
      // Filter out weekly restarts, and weird dates (< 2000)
      const TimeStamp = new Date(line.split(",")[0]);
      return (
        TimeStamp.getTime() > 0 &&
        !(
          ((TimeStamp.getHours() == 2 &&
            (TimeStamp.getMinutes() == 0 ||
              TimeStamp.getMinutes() == 1 ||
              TimeStamp.getMinutes() == 2)) ||
            (TimeStamp.getHours() == 1 && TimeStamp.getMinutes() == 59)) &&
          line.includes("0,0,0,0,0,0,0")
        )
      );
    })
    .map((line) => line.trim().toLowerCase().split(","));

  function newDataSet(label: string, color: string, yAxis: string): myDataSet {
    return {
      label: label,
      backgroundColor: color,
      borderColor: color,
      data: [] as any[],
      yAxisID: yAxis,
    };
  }

  let newDatasets = [] as myDataSet[];
  let availibleColors = [
    "yellow",
    "red",
    "cyan",
    "darkorange",
    "cornflowerblue",
    "lime",
    "magenta",
    "whitesmoke",
    "steelblue",
    "lightsteelblue",
  ];
  for (let index = 1; index < HeaderLine.length; index++) {
    // not a standard dataset
    const DataColor =
      availibleColors.length > 0
        ? (availibleColors.pop() as string)
        : "rgb(" +
          Math.round(Math.random() * 255) +
          "," +
          Math.round(Math.random() * 255) +
          "," +
          Math.round(Math.random() * 255) +
          ")";
    newDatasets.push(
      newDataSet(
        HeaderLine[index],
        DataColor,
        HeaderLine[index].substring(0, 5),
      ),
    );
    // eslint-disable-next-line @typescript-eslint/ban-ts-comment
    // @ts-ignore
    chart.config.options[HeaderLine[index].substring(0, 5)] = {
      display: true,
      position: "right",
      ticks: { color: DataColor, maxRotation: 0 },
      grid: { drawOnChartArea: false },
    };

    data.forEach((line, _) => {
      newDatasets[newDatasets.length - 1].data.push({
        x: Date.parse(line[0]),
        y: Number(line[index].replace(/[^0-9.]/g, "")),
      });
    });

    if (chart.config.options?.scales?.x != null) {
      chart.config.options.scales.x.min = newDatasets[0].data[0].x;
      chart.config.options.scales.x.max =
        newDatasets[0].data[newDatasets[0].data.length - 1].x;
    }
  }

  for (let set of newDatasets) {
    chart.config.data.datasets.push(set as any);
  }

  chart.update();
}
