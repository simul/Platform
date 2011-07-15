var mvMatrix = mat4.create();
var modelMatrix = mat4.create();
var viewMatrix = mat4.create();
var matrixStack = [];
var pMatrix = mat4.create();

function pushMatrix(mat)
{
	var copy = mat4.create();
	mat4.set(mat, copy);
	matrixStack.push(copy);
}

function popMatrix()
{
	if (matrixStack.length == 0)
	{
		throw "Invalid popMatrix!";
	}
	return matrixStack.pop();
}

function degToRad(degrees)
{
	return degrees * Math.PI / 180;
}

var gl;
function initGL(canvas)
{
	try
	{
		gl=canvas.getContext("experimental-webgl",{antialias:true});
		gl.viewportWidth=canvas.width;
		gl.viewportHeight=canvas.height;
	}
	catch (e)
	{
	}
	if(!gl)
	{
		alert("Could not initialise WebGL :-( Try Firefox Beta or Chrome browser!");
	}
}

var triangleVertexPositionBuffer;
var triangleVertexColourBuffer;
var squareVertexPositionBuffer;
var squareVertexColourBuffer;

var lastTime=0;
var timeStep=0;

function firstOrderDecay(value,target,rate)
{
	retain=1.0/(1.0+rate);
	intro=1.0-retain;
	value=value*retain;
	value=value+intro*target;
	return value;
}

function animate()
{
	var timeNow = new Date().getTime();
	if (lastTime != 0)
	{
		timeStep = (timeNow - lastTime)/ 1000.0;
	}
	lastTime = timeNow;
	
	interp=firstOrderDecay(interp,target_interp,0.1);
}

var yaw		=0;
var pitch	=0;

function drawScene()
{
	gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
	
	mat4.perspective(45,gl.viewportWidth/gl.viewportHeight,0.1,100000.0,pMatrix);
	mat4.identity(modelMatrix);
	
	mat4.identity(viewMatrix);
	mat4.translate(viewMatrix, [0,0,0]);
	mat4.rotate(viewMatrix, degToRad(90-pitch), [-1, 0, 0]);
    mat4.rotate(viewMatrix, degToRad(yaw), [0, 0, 1]);

	mat4.multiply(viewMatrix,modelMatrix,mvMatrix);

	mercury=getMercury();
	venus=getVenus();
	earth=getEarth();
	mars=getMars();
	jupiter=getJupiter();
	saturn=getSaturn();
	uranus=getUranus();
	neptune=getNeptune();
	
	gl.useProgram(lineProgram);
	//drawCircle(earth[1]);

	grey=[.5,.5,.5];
	yellow=[1.0,1.0,.5];
	
	drawSky();
	drawClouds();
	drawSphere(skyProgram,[0,0,0], 0.4	,[0.2,0.4,1.0]);
}

function setSchematic()
{
	target_interp=0.0;
	schematic=document.getElementById("schematic");
	schematic.setActive(true);
	to_scale=document.getElementById("to_scale");
	to_scale.setActive(false);
}

function setToScale()
{
	target_interp=1.0;
	schematic=document.getElementById("schematic");
	schematic.setActive(false);
	to_scale=document.getElementById("to_scale");
	to_scale.setActive(true);
}

function tick()
{
	jdn=jdn+timeStep*jdn_rate;
	requestAnimFrame(tick);
	drawScene();
	animate();
}

function cmfn()
{
	return stopNativeContent();
}
function onResize()
{
	var canvas_holder = document.getElementById("canvas_holder");
	var canvas = document.getElementById("simul_canvas");
	canvas.width  = canvas_holder.width;
	canvas.height = canvas.width;
	initGL(canvas);
    /*var size = Math.min(window.innerWidth, window.innerHeight) - 10;
    canvas.width=size;
	canvas.height=size;
    gl.viewport(0, 0, size, size);
    drawScene();*/
}
		
function trueSkyStart()
{
	$(this)[0].oncontextmenu = function() {
        return false;
	}
	var canvas = document.getElementById("simul_canvas");
	initGL(canvas);
	if(!gl)
		return;
	initShaders();
	initLineArt();
	initSphere();
	gl.clearColor(0.0, 0.0, 0.0, 1.0);
	gl.enable(gl.DEPTH_TEST);

    canvas.onmousedown = handleMouseDown;
    document.onmouseup = handleMouseUp;
    document.onmousemove = handleMouseMove;
    document.onmouseout = handleMouseUp;
	if (document.layers)
		window.captureEvents(Event.MOUSEDOWN);
	if (document.layers)
		window.captureEvents(Event.MOUSEUP);

	tick();
}