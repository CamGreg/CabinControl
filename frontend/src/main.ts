import './style.css'; // Your main CSS file
import Chart from 'chart.js/auto'; // Import Chart.js

document.addEventListener('DOMContentLoaded', () => {
    const currentMonthDisplay = document.getElementById('current-month-display');
    const chartCanvas = document.getElementById('myChart');

    if (!chartCanvas) {
        console.error('Chart canvas not found!');
        return;
    }

    const ctx = chartCanvas.getContext('2d');
    let myChart; // To store chart instance for later updates/destruction

    async function fetchCsvData() {
        const now = new Date();
        const year = now.getFullYear();
        const month = String(now.getMonth() + 1).padStart(2, '0'); // Month is 0-indexed

        const filename = `${year}-${month}.csv`;
        const csvUrl = `/data/${filename}`; // Matches your ESP32 endpoint

        currentMonthDisplay.textContent = `${year}-${month}`;
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
            console.log('CSV data fetched successfully.');
            return parseCsv(csvText);

        } catch (error) {
            console.error("Could not fetch CSV data:", error);
            alert(`Failed to load data: ${error.message || error}. Please ensure the ESP32 is running and the file exists.`);
            return null;
        }
    }

    function parseCsv(csvText: any) {
        const lines = csvText.trim().split('\n');
        if (lines.length < 2) {
            console.warn("CSV data too short or empty after trimming.");
            return { labels: [], data1: [], data2: [], data3: [] }; // Return empty data
        }

        const headers = lines[0].split(',');
        // Assuming headers: Timestamp,Temperature,Humidity,Pressure
        const timestampIndex = headers.indexOf('Timestamp');
        const tempIndex = headers.indexOf('Temperature');
        const humidityIndex = headers.indexOf('Humidity');
        const pressureIndex = headers.indexOf('Pressure');

        if (timestampIndex === -1 || tempIndex === -1 || humidityIndex === -1 || pressureIndex === -1) {
            console.error("Missing expected headers in CSV.");
            // Handle missing headers gracefully, maybe alert user or use default indices
            // For now, let's assume fixed indices if headers are missing for demo purposes
            // (You should make this more robust in production)
            // Or throw an error: throw new Error("CSV headers do not match expected format.");
            return { labels: [], data1: [], data2: [], data3: [] };
        }


        const labels = [];
        const temperatureData = [];
        const humidityData = [];
        const pressureData = [];

        for (let i = 1; i < lines.length; i++) {
            const parts = lines[i].split(',');
            if (parts.length > Math.max(timestampIndex, tempIndex, humidityIndex, pressureIndex)) {
                labels.push(parts[timestampIndex]);
                temperatureData.push(parseFloat(parts[tempIndex]));
                humidityData.push(parseFloat(parts[humidityIndex]));
                pressureData.push(parseFloat(parts[pressureIndex]));
            }
        }
        return {
            labels: labels,
            temperature: temperatureData,
            humidity: humidityData,
            pressure: pressureData
        };
    }

    function renderChart(data) {
        if (myChart) {
            myChart.destroy(); // Destroy previous chart instance if it exists
        }

        myChart = new Chart(ctx, {
            type: 'line', // Or 'bar', 'scatter'
            data: {
                labels: data.labels, // Timestamps from CSV
                datasets: [
                    {
                        label: 'Temperature (°C)',
                        data: data.temperature,
                        borderColor: 'rgb(255, 99, 132)',
                        backgroundColor: 'rgba(255, 99, 132, 0.2)',
                        tension: 0.1,
                        fill: false
                    },
                    {
                        label: 'Humidity (%)',
                        data: data.humidity,
                        borderColor: 'rgb(54, 162, 235)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        tension: 0.1,
                        fill: false
                    },
                    {
                        label: 'Pressure (hPa)',
                        data: data.pressure,
                        borderColor: 'rgb(75, 192, 192)',
                        backgroundColor: 'rgba(75, 192, 192, 0.2)',
                        tension: 0.1,
                        fill: false,
                        yAxisID: 'y1' // Use a secondary Y-axis for pressure if scales are very different
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false, // Important for controlling size with CSS
                scales: {
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: { // Primary Y-axis for Temperature and Humidity
                        beginAtZero: false,
                        title: {
                            display: true,
                            text: 'Value'
                        }
                    },
                    y1: { // Secondary Y-axis for Pressure
                        type: 'linear',
                        display: true,
                        position: 'right',
                        grid: {
                            drawOnChartArea: false, // Only draw grid lines for the primary Y axis
                        },
                        title: {
                            display: true,
                            text: 'Pressure'
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: true,
                        position: 'top'
                    },
                    title: {
                        display: true,
                        text: 'Monthly Sensor Data Overview'
                    }
                }
            }
        });
    }

    // Initial data load and chart render
    fetchCsvData().then(data => {
        if (data && data.labels.length > 0) {
            renderChart(data);
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