"use strict";
// global variables
const X_RESOLUTION = 128; // physical screen x resolution
const Y_RESOLUTION = 64; // physical screen y resolution
let CANVAS_SIZE = 2; // canvas pixel size
const ICON_SIZE = 16; // icon pixel size
const DEFAULT_SIZE = 1; // physical screen default pixel size
const DEFAULT_FRAMETIME = 16; // physical screen default target frametime

// when the page has loaded, initialize the grid
document.addEventListener('DOMContentLoaded', () => {
    refreshGrid();
    drawButtons();
    document.getElementById('size').value = DEFAULT_SIZE;
    document.getElementById('frametime').value = DEFAULT_FRAMETIME;
}, false);

// draw icons on the button canvas'
const drawButtons = () => {
    let canvas, context;
    // clear button is left clear (but sized, just in case)
    canvas = document.getElementById('btn-clear');
    canvas.width = 64;
    canvas.height = 64;
    // random button is filled randomly
    canvas = document.getElementById('btn-random');
    canvas.width = 64;
    canvas.height = 64;
    context = canvas.getContext('2d');
    context.fillStyle = '#fff';
    for (let y = 0; y < canvas.height / ICON_SIZE; y++) {
        for (let x = 0; x < canvas.width / ICON_SIZE; x++) {
            if (Math.random() >= 0.5) context.fillRect(x*ICON_SIZE, y*ICON_SIZE, ICON_SIZE, ICON_SIZE);
        }
    }
    // glider button has a glider
    canvas = document.getElementById('btn-glider');
    canvas.width = 64;
    canvas.height = 64;
    context = canvas.getContext('2d');
    context.fillStyle = '#fff';
    context.fillRect(2*ICON_SIZE, 1*ICON_SIZE, ICON_SIZE, ICON_SIZE);
    context.fillRect(3*ICON_SIZE, 2*ICON_SIZE, ICON_SIZE, ICON_SIZE);
    context.fillRect(1*ICON_SIZE, 3*ICON_SIZE, 3*ICON_SIZE, ICON_SIZE);
    // refresh button has a circle
    canvas = document.getElementById('btn-refresh');
    canvas.width = 64;
    canvas.height = 64;
    context = canvas.getContext('2d');
    context.beginPath();
    context.lineWidth = ICON_SIZE;
    context.strokeStyle = 'white';
    context.rect(ICON_SIZE/2, ICON_SIZE/2, 64-ICON_SIZE, 64-ICON_SIZE);
    context.stroke();
};

// draw the grid data on the canvas
const drawGrid = (gridData) => {
    const size = document.getElementById('size').value;
    const canvas = document.getElementById('grid');
    // resetting the size also clears the grid
    canvas.width = X_RESOLUTION * CANVAS_SIZE;
    canvas.height = Y_RESOLUTION * CANVAS_SIZE;
    const context = canvas.getContext('2d');
    context.fillStyle = '#fff';
    // if we have valid data, draw it on the grid
    if (gridData.length === Y_RESOLUTION / size && gridData[0].length === X_RESOLUTION / size) {
        for (let y = 0; y < Y_RESOLUTION / size; y++) {
            for (let x = 0; x < X_RESOLUTION / size; x++) {
                if (gridData[y][x]) context.fillRect(x*CANVAS_SIZE*size, y*CANVAS_SIZE*size, CANVAS_SIZE*size, CANVAS_SIZE*size);
            }
        }
    }
};

// fetch the grid data and call drawGrid with it
const refreshGrid = async () => {
    const size = document.getElementById('size').value;
    const frametime = document.getElementById('frametime').value;
    let gridData = await fetch(`/getGrid?size=${size}&frametime=${frametime}`).then(response => {
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