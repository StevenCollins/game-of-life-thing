"use strict";
// global variables
const X_RESOLUTION = 128;
const Y_RESOLUTION = 64;
let SIZE = 2;

// when the page has loaded, initialize the grid
document.addEventListener('DOMContentLoaded', () => {
    refreshGrid();
}, false);

// draw the grid data on the canvas
const drawGrid = (gridData) => {
    const canvas = document.getElementById('grid');
    const context = canvas.getContext('2d');
    // resetting the size also clears the grid
    canvas.width = X_RESOLUTION * SIZE;
    canvas.height = Y_RESOLUTION * SIZE;
    // if we have valid data, draw it on the grid
    if (gridData.length === Y_RESOLUTION && gridData[0].length === X_RESOLUTION) {
        for (let y = 0; y < Y_RESOLUTION; y++) {
            for (let x = 0; x < X_RESOLUTION; x++) {
                if (gridData[y][x]) context.fillRect(x*SIZE, y*SIZE, SIZE, SIZE);
            }
        }
    }
};

// fetch the grid data and call drawGrid with it
const refreshGrid = async () => {
    let gridData = await fetch('/getGrid').then(response => {
        if (response.ok) {
            return response.json();
        } else {
            return [];
        }
    });
    drawGrid(gridData);
};

// hit the specified endpoint and refresh the grid
const fetchAndRefresh = (endpoint) => {
    fetch(endpoint);
    refreshGrid();
}