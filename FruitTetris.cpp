/*
CMPT 361 Assignment 1 - FruitTetris implementation Sample Skeleton Code

- This is ONLY a skeleton code showing:
How to use multiple buffers to store different objects
An efficient scheme to represent the grids and blocks

- Compile and Run:
Type make in terminal, then type ./FruitTetris

This code is extracted from Connor MacLeod's (crmacleo@sfu.ca) assignment submission
by Rui Ma (ruim@sfu.ca) on 2014-03-04. 

Modified in Sep 2014 by Honghua Li (honghual@sfu.ca).
*/

#include "include/Angel.h"
#include <cstdlib>
#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace std;

#define TILE_DROP_SPEED 500
#define TILE_DROP_SPEED_FAST 20
#define MAX_TILE_ORIENTATIONS 4
#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define MAX_FRUIT_GROUP 3

// forward declarations
void checkGroupedFruits(const vec2 &p);
void tileDrop(int type);
void rotateCurrentTile(int n);
bool isInBoardBounds(vec2 p);
bool isInBoardBounds(int x, int y);
enum TileInfo {
	TILE_CREATE,
	TILE_TICK,
	TILE_TICK_FAST,
	TILE_COLUMN_CHECK
};
int numCheckFruitColumnCallbacks = 0;
vector<vec2> checkFruitColumnArgs;
vector<vec2> removedTiles;
set<int> columnDone;
int numDropTileCallbacks = 0;
int numFastDropTileCallbacks = 0;

// vector sorting
struct sortByDecY { bool operator() (vec2 const &L, vec2 const &R) { return L.y > R.y; } };
struct sortByIncY { bool operator() (vec2 const &L, vec2 const &R) { return L.y < R.y; } };

void printVec4(const vec4 &f) {
	cout.precision(6);
	cout << fixed << "Vec4: " << f.x << " " << f.y << " " <<f.z <<" "<<f.w << endl;
}
void printVec2(const vec2 &f) { cout << "(" << f.x << "," << f.y << ")"; }
bool vec4Equal(const vec4 &a, const vec4 &b) {
	return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w;
}

// xsize and ysize represent the window size - updated if window is reshaped to prevent stretching of the game
int xsize = 400; 
int ysize = 720;

int tileDropSpeed = TILE_DROP_SPEED;
// current tile
vec2 currTileOffset[4]; // An array of 4 2d vectors representing displacement from a 'center' piece of the tile, on the grid
vec2 currTilePos = vec2(5, BOARD_HEIGHT - 1); // The position of the current tile using grid coordinates ((0,0) is the bottom left corner)
int currTileShapeIndex = 0;
vec4 currTileColours[4];

//-------------------------------------------------------------------------------------------------------------------

enum TileShape {
	TileShapeI,
	TileShapeS,
	TileShapeL,
	MaxTileShapes
};

const vec2 allShapes[MaxTileShapes][4] =
	{{vec2(-2, 0), vec2(-1, 0), vec2(0, 0), vec2(1, 0)}, // I
	{vec2(-1, -1), vec2(0, -1), vec2(0, 0), vec2(1, 0)}, // S
	{vec2(-1, -1), vec2(-1,0), vec2(0, 0), vec2(1, 0)}}; // L


//-------------------------------------------------------------------------------------------------------------------

// colors
const vec4 white  = vec4(1.0, 1.0, 1.0, 1.0);
const vec4 black  = vec4(0.0, 0.0, 0.0, 1.0); 
const vec4 cellFreeColour = vec4(white);

// fruit colors: https://kuler.adobe.com/create/color-wheel/?base=2&rule=Custom&selected=3&name=My%20Kuler%20Theme&mode=rgb&rgbvalues=1,0.8626810137791381,0,0.91,0.5056414909356977,0,1,0.10293904996979109,0,0.5587993310653088,0,0.91,0.1658698853207745,1,0.10159077034733333&swatchOrder=0,1,2,3,4
const vec4 grape  = vec4(142/255.0 , 54/255.0  , 232/255.0 , 1.0);
const vec4 apple  = vec4(255/255.0 , 26/255.0  , 0/255.0  , 1.0);
const vec4 banana = vec4(255/255.0 , 220/255.0 , 0/255.0  , 1.0);
const vec4 pear   = vec4(42/255.0  , 255/255.0 , 26/255.0  , 1.0);
const vec4 orange = vec4(232/255.0 , 129/255.0 , 0/255.0   , 1.0);

enum FruitColours {
	ColourGrape,
	ColourApple,
	ColourBanana,
	ColourPear,
	ColourOrange,
	MaxFruitColours
};

const vec4 fruitColours[] = {grape, apple, banana, pear, orange};

//-------------------------------------------------------------------------------------------------------------------
 
//board[x][y] represents whether the cell (x,y) is occupied
bool board[BOARD_WIDTH][BOARD_HEIGHT]; 

void setCellOccupied(const vec2 &p, bool o) {
	board[(int)p.x][(int)p.y] = o;
}
bool isCellOccupied(int x, int y) {
	return board[x][y];
}
bool isCellOccupied(const vec2 &p) {
	return board[(int)p.x][(int)p.y];
}

//An array containing the colour of each of the 10*20*2*3 vertices that make up the board
//Initially, all will be set to black. As tiles are placed, sets of 6 vertices (2 triangles; 1 square)
//will be set to the appropriate colour in this array before updating the corresponding VBO
vec4 boardcolours[1200];

// location of vertex attributes in the shader program
GLuint vPosition;
GLuint vColor;

// locations of uniform variables in shader program
GLuint locxsize;
GLuint locysize;

// VAO and VBO
GLuint vaoIDs[3]; // One VAO for each object: the grid, the board, the current piece
enum VBO_IDs {
	GridPositionBO,
	GridColourBO,
	BoardPositionBO,
	BoardColourBO,
	CurrentTilePositionBO,
	CurrentTileColourBO
};
GLuint vboIDs[6]; // Two Vertex Buffer Objects for each VAO (specifying vertex positions and colours, respectively)

//-------------------------------------------------------------------------------------------------------------------

// When the current tile is moved or rotated (or created), update the VBO containing its vertex position data
void updatetile()
{
	// Bind the VBO containing current tile vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[4]); 

	// For each of the 4 'cells' of the tile,
	for (int i = 0; i < 4; i++) 
	{
		// Calculate the grid coordinates of the cell
		GLfloat x = currTilePos.x + currTileOffset[i].x; 
		GLfloat y = currTilePos.y + currTileOffset[i].y;

		// Create the 4 corners of the square - these vertices are using location in pixels
		// These vertices are later converted by the vertex shader
		vec4 p1 = vec4(33.0 + (x * 33.0), 33.0 + (y * 33.0), .4, 1); 
		vec4 p2 = vec4(33.0 + (x * 33.0), 66.0 + (y * 33.0), .4, 1);
		vec4 p3 = vec4(66.0 + (x * 33.0), 33.0 + (y * 33.0), .4, 1);
		vec4 p4 = vec4(66.0 + (x * 33.0), 66.0 + (y * 33.0), .4, 1);

		// Two points are used by two triangles each
		vec4 newpoints[6] = {p1, p2, p3, p2, p3, p4}; 

		// Put new data in the VBO
		glBufferSubData(GL_ARRAY_BUFFER, i*6*sizeof(vec4), 6*sizeof(vec4), newpoints); 
	}

	glBindVertexArray(0);
}

//-------------------------------------------------------------------------------------------------------------------

// Called to keep the tile within the bounds of the board by nudging the tile into place
bool nudgeCurrentTile(const vec2 *o) {
	for(int i = 0; i < 4; i++) {
		vec2 p = currTilePos + o[i];
		if(isInBoardBounds(p) && isCellOccupied(p)) return false;
	}
	for(int i = 0; i < 4; i++) {
		vec2 p = currTilePos + o[i];
		if(!isInBoardBounds(p)) currTilePos -= o[i];
	}
	return true;
}
bool nudgeCurrentTile(int cellOffsetX, int cellOffsetY) {
	int cellX = currTilePos.x + cellOffsetX;
	int cellY = currTilePos.y + cellOffsetY;
	if(isInBoardBounds(cellX,cellY) && isCellOccupied(cellX, cellY)) return false;
	currTilePos.x -= cellX<0 ? cellOffsetX : cellX>BOARD_WIDTH - 1 ? cellOffsetX : 0;
	currTilePos.y -= cellY>BOARD_HEIGHT - 1 ? cellOffsetY : cellY<0 ? cellOffsetY : 0;
	return true;
}

//-------------------------------------------------------------------------------------------------------------------

void shuffleAndUpdateColours() {
	vec4 temp = currTileColours[0];
	for(int i = 0; i < 4 - 1; i++)
		currTileColours[i] = currTileColours[i + 1];
	currTileColours[3] = temp;
	
	// Update the color VBO of current tile
	vec4 newcolours[24];
	for (int i = 0; i < 24; i++)
		newcolours[i] = currTileColours[i/6];

	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[5]); // Bind the VBO containing current tile vertex colours
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(newcolours), newcolours); // Put the colour data in the VBO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//-------------------------------------------------------------------------------------------------------------------

// Called at the start of play and every time a tile is placed
void newtile() {
	tileDropSpeed = TILE_DROP_SPEED;
	//currTilePos = vec2(5 , BOARD_HEIGHT); // Put the tile at the top of the board
	currTilePos = vec2(rand() % BOARD_WIDTH, BOARD_HEIGHT - 1); // Put the tile at the top of the board

	// Update the geometry VBO of current tile
	currTileShapeIndex = rand() % MaxTileShapes;
	for(int i = 0; i < 4; i++) {
		currTileColours[i] = fruitColours[rand() % 2];
		currTileOffset[i] = allShapes[currTileShapeIndex][i];
		nudgeCurrentTile(currTileOffset[i].x, currTileOffset[i].y);
	}
	rotateCurrentTile(rand() % 5);
	shuffleAndUpdateColours();
	updatetile(); 

	glBindVertexArray(0);
}

//-------------------------------------------------------------------------------------------------------------------

void initGrid() {
	// ***Generate geometry data
	vec4 gridpoints[64]; // Array containing the 64 points of the 32 total lines to be later put in the VBO
	vec4 gridcolours[64]; // One colour per vertex
	// Vertical lines 
	for (int i = 0; i < 11; i++){
		gridpoints[2*i] = vec4((33.0 + (33.0 * i)), 33.0, 0, 1);
		gridpoints[2*i + 1] = vec4((33.0 + (33.0 * i)), 693.0, 0, 1);
		
	}
	// Horizontal lines
	for (int i = 0; i < 21; i++){
		gridpoints[22 + 2*i] = vec4(33.0, (33.0 + (33.0 * i)), 0, 1);
		gridpoints[22 + 2*i + 1] = vec4(363.0, (33.0 + (33.0 * i)), 0, 1);
	}
	// Make all grid lines white
	for (int i = 0; i < 64; i++)
		gridcolours[i] = white;


	// *** set up buffer objects
	// Set up first VAO (representing grid lines)
	glBindVertexArray(vaoIDs[0]); // Bind the first VAO
	glGenBuffers(2, vboIDs); // Create two Vertex Buffer Objects for this VAO (positions, colours)

	// Grid vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[0]); // Bind the first grid VBO (vertex positions)
	glBufferData(GL_ARRAY_BUFFER, 64*sizeof(vec4), gridpoints, GL_STATIC_DRAW); // Put the grid points in the VBO
	glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, 0, 0); 
	glEnableVertexAttribArray(vPosition); // Enable the attribute
	
	// Grid vertex colours
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[1]); // Bind the second grid VBO (vertex colours)
	glBufferData(GL_ARRAY_BUFFER, 64*sizeof(vec4), gridcolours, GL_STATIC_DRAW); // Put the grid colours in the VBO
	glVertexAttribPointer(vColor, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vColor); // Enable the attribute
}


void initBoard() {
	// *** Generate the geometric data
	vec4 boardpoints[1200];
	for (int i = 0; i < 1200; i++)
		boardcolours[i] = cellFreeColour; // Let the empty cells on the board be black
	// Each cell is a square (2 triangles with 6 vertices)
	for (int i = 0; i < BOARD_HEIGHT; i++){
		for (int j = 0; j < BOARD_WIDTH; j++)
		{
			vec4 p1 = vec4(33.0 + (j * 33.0), 33.0 + (i * 33.0), .5, 1); // bottom left
			vec4 p2 = vec4(33.0 + (j * 33.0), 66.0 + (i * 33.0), .5, 1); // top left
			vec4 p3 = vec4(66.0 + (j * 33.0), 33.0 + (i * 33.0), .5, 1); // bottom right
			vec4 p4 = vec4(66.0 + (j * 33.0), 66.0 + (i * 33.0), .5, 1); // top right
			
			// Two points are reused
			boardpoints[6*(BOARD_WIDTH*i + j)    ] = p1;
			boardpoints[6*(BOARD_WIDTH*i + j) + 1] = p2;
			boardpoints[6*(BOARD_WIDTH*i + j) + 2] = p3;
			boardpoints[6*(BOARD_WIDTH*i + j) + 3] = p2;
			boardpoints[6*(BOARD_WIDTH*i + j) + 4] = p3;
			boardpoints[6*(BOARD_WIDTH*i + j) + 5] = p4;
		}
	}

	// Initially no cell is occupied
	for (int i = 0; i < BOARD_WIDTH; i++)
		for (int j = 0; j < BOARD_HEIGHT; j++)
			board[i][j] = false; 


	// *** set up buffer objects
	glBindVertexArray(vaoIDs[1]);
	glGenBuffers(2, &vboIDs[2]);

	// Grid cell vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[2]);
	glBufferData(GL_ARRAY_BUFFER, 1200*sizeof(vec4), boardpoints, GL_STATIC_DRAW);
	glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vPosition);

	// Grid cell vertex colours
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[3]);
	glBufferData(GL_ARRAY_BUFFER, 1200*sizeof(vec4), boardcolours, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(vColor, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vColor);
}

// No geometry for current tile initially
void initCurrentTile()
{
	glBindVertexArray(vaoIDs[2]);
	glGenBuffers(2, &vboIDs[4]);

	// Current tile vertex positions
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[4]);
	glBufferData(GL_ARRAY_BUFFER, 24*sizeof(vec4), NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(vPosition, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vPosition);

	// Current tile vertex colours
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[5]);
	glBufferData(GL_ARRAY_BUFFER, 24*sizeof(vec4), NULL, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(vColor, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(vColor);
}

void init()
{
	// Load shaders and use the shader program
	GLuint program = InitShader("vshader.glsl", "fshader.glsl");
	glUseProgram(program);

	// Get the location of the attributes (for glVertexAttribPointer() calls)
	vPosition = glGetAttribLocation(program, "vPosition");
	vColor = glGetAttribLocation(program, "vColor");

	// Create 3 Vertex Array Objects, each representing one 'object'. Store the names in array vaoIDs
	glGenVertexArrays(3, &vaoIDs[0]);

	// Initialize the grid, the board, and the current tile
	initGrid();
	initBoard();
	initCurrentTile();

	// The location of the uniform variables in the shader program
	locxsize = glGetUniformLocation(program, "xsize"); 
	locysize = glGetUniformLocation(program, "ysize");

	// Game initialization
	newtile(); // create new next tile

	// set to default
	glBindVertexArray(0);
	glClearColor(0, 0, 0, 0);
}

//-------------------------------------------------------------------------------------------------------------------

bool isInBoardBounds(vec2 p) {
	if(p.x < 0 || p.x > BOARD_WIDTH - 1)
		return false;
	if(p.y < 0 || p.y > BOARD_HEIGHT - 1)
		return false;
	return true;
}
bool isInBoardBounds(int x, int y) {
	return isInBoardBounds(vec2(x, y));
}

// Rotates the current tile, nudge to make room
void rotateCurrentTile(int n) {
	vec2 nextOrientation[4];
	for(int i = 0; i < 4; i++) nextOrientation[i] = vec2(-currTileOffset[i].y, currTileOffset[i].x);
	for(int k = 0; k < n; k++) for(int i = 0; i < 4; i++) nextOrientation[i] = vec2(-nextOrientation[i].y, nextOrientation[i].x);
	if(!nudgeCurrentTile(nextOrientation)) return;
	for(int i = 0; i < 4; i++) currTileOffset[i] = nextOrientation[i];
}

//-------------------------------------------------------------------------------------------------------------------
int ix = 0; int iy=0;
void setCellColour(const vec2 &p, const vec4 &c) {
	boardcolours[6*(BOARD_WIDTH*(int)p.y + (int)p.x)    ] = c;
	boardcolours[6*(BOARD_WIDTH*(int)p.y + (int)p.x) + 1] = c;
	boardcolours[6*(BOARD_WIDTH*(int)p.y + (int)p.x) + 2] = c;
	boardcolours[6*(BOARD_WIDTH*(int)p.y + (int)p.x) + 3] = c;
	boardcolours[6*(BOARD_WIDTH*(int)p.y + (int)p.x) + 4] = c;
	boardcolours[6*(BOARD_WIDTH*(int)p.y + (int)p.x) + 5] = c;
	ix = p.x;
	iy = p.y;
}

// Places the current tile - update the board vertex colour VBO and the array maintaining occupied cells
void setTileColour(const vec2 &p) {
	for(int i = 0; i < 4; i++) {
		int cellX = p.x + currTileOffset[i].x;
		int cellY = p.y + currTileOffset[i].y;
		board[cellX][cellY] = true;
		setCellColour(vec2(cellX, cellY), currTileColours[i]);
	}
}

void updateBoard() {
	glBindBuffer(GL_ARRAY_BUFFER, vboIDs[BoardColourBO]); 
	glBufferData(GL_ARRAY_BUFFER, 1200*sizeof(vec4), boardcolours, GL_DYNAMIC_DRAW);
}

//-------------------------------------------------------------------------------------------------------------------

// Given (x,y), tries to move the tile x squares to the right and y squares down
// Returns true if the tile was successfully moved, or false if there was some issue
bool moveTile(vec2 direction) {
	for (int i = 0; i < 4; i++) {
		int cellX = currTilePos.x + currTileOffset[i].x + direction.x;
		int cellY = currTilePos.y + currTileOffset[i].y + direction.y;
		if(!isInBoardBounds(vec2(cellX, cellY)))
			return false;
		if(board[cellX][cellY] == true)
			return false;
	}
	return true;
}
//-------------------------------------------------------------------------------------------------------------------

// Starts the game over - empties the board, creates new tiles, resets line counters
void restart()
{
	checkFruitColumnArgs.clear();
	numDropTileCallbacks = numFastDropTileCallbacks = numCheckFruitColumnCallbacks = 0;
	numDropTileCallbacks++;
	initBoard();
	newtile();
}
//-------------------------------------------------------------------------------------------------------------------
float x = -1.0f;
float y = 0.7f;
// Draws the game
void display()
{

	glClear(GL_COLOR_BUFFER_BIT);

	glUniform1i(locxsize, xsize); // x and y sizes are passed to the shader program to maintain shape of the vertices on screen
	glUniform1i(locysize, ysize);

	glBindVertexArray(vaoIDs[1]); // Bind the VAO representing the grid cells (to be drawn first)
	glDrawArrays(GL_TRIANGLES, 0, 1200); // Draw the board (10*20*2 = 400 triangles)

	glBindVertexArray(vaoIDs[2]); // Bind the VAO representing the current tile (to be drawn on top of the board)
	glDrawArrays(GL_TRIANGLES, 0, 24); // Draw the current tile (8 triangles)

	glBindVertexArray(vaoIDs[0]); // Bind the VAO representing the grid lines (to be drawn on top of everything else)
	glDrawArrays(GL_LINES, 0, 64); // Draw the grid lines (21+11 = 32 lines)

	x+=0.01;
	glRasterPos2f(x, y);
	glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'h');
	glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, 'k');

	cout << "start test:" << endl;
	stringstream ss;
	ss << currTilePos.x;
	glRasterPos2f(0, 0);
	glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
	char c;
	while(ss >> c) {
		cout << c << endl;
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
	}
	glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ',');
	ss.clear(); ss.str("");
	ss << currTilePos.y;
	while(ss >> c) {
		cout << c << endl;
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
	}
	cout << "end test:" << endl;
	glutSwapBuffers();
}

//-------------------------------------------------------------------------------------------------------------------

// Reshape callback will simply change xsize and ysize variables, which are passed to the vertex shader
// to keep the game the same from stretching if the window is stretched
void reshape(GLsizei w, GLsizei h)
{
	xsize = w;
	ysize = h;
	glViewport(0, 0, w, h);
}

//-------------------------------------------------------------------------------------------------------------------
// Handle arrow key keypresses
void special(int key, int x, int y)
{
	switch(key) {
		case GLUT_KEY_UP:
			rotateCurrentTile(2);
			updatetile();
			break;
		case GLUT_KEY_DOWN:
			if(numFastDropTileCallbacks == 0) {
				numFastDropTileCallbacks++;
				tileDrop(TILE_TICK_FAST);
			}
			break;
		case GLUT_KEY_RIGHT:
			if(moveTile(vec2(1, 0))) {
				currTilePos.x += 1;
				updatetile();
			}
			break;
		case GLUT_KEY_LEFT:
			if(moveTile(vec2(-1, 0))) {
				currTilePos.x -= 1;
				updatetile();
			}
			break;
		default:
			break;
	}
}

//-------------------------------------------------------------------------------------------------------------------

// Handles standard keypresses
void keyboard(unsigned char key, int x, int y)
{
	switch(key) 
	{
		case 033: // Both escape key and 'q' cause the game to exit
		    exit(EXIT_SUCCESS);
		    break;
		case 'q':
			exit (EXIT_SUCCESS);
			break;
		case 'r': // 'r' key restarts the game
			restart();
			break;
		case ' ':
			shuffleAndUpdateColours();
			updatetile();
			break;
	}
	glutPostRedisplay();
}

//-------------------------------------------------------------------------------------------------------------------

void idle(void)
{
	glutPostRedisplay();
}

bool cellFreeToFall(const vec2 &p) {
		if((int)p.y - 1 < 0)
			return false;
		return !isCellOccupied(p - vec2(0, 1));
}

bool tileFreeToFall(const vec2 &p) {
	for(int i = 0; i < 4; i++) {
		if(!cellFreeToFall(p + currTileOffset[i]))
			return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------------------------------------

vec4 getCellColour(const vec2 &p) {
	return boardcolours[6*(10*(int)p.y + (int)p.x)];
}

void removeTileFromBoard(const vec2 &p) {
	setCellOccupied(p, false);
	setCellColour(p, cellFreeColour);
	updateBoard();
}

// true if horizontal group, false if vertical or fail, c can be null if we just want to know the type of group
void recursiveCheck(const vec2 &p, const vec2 &dir, vector<vec2> *h, vector<vec2> *v) {
	if(dir.x == 0 && dir.y == 0 && isInBoardBounds(p)) {
		vector<vec2> vertGroup; vertGroup.push_back(vec2(p));
		vector<vec2> horzGroup; horzGroup.push_back(vec2(p));
		recursiveCheck(p, vec2(0, -1), NULL, &vertGroup); recursiveCheck(p, vec2(0, 1), NULL, &vertGroup);
		recursiveCheck(p, vec2(1, 0), &horzGroup, NULL); recursiveCheck(p, vec2(-1, 0), &horzGroup, NULL);
		h->swap(horzGroup); v->swap(vertGroup);
	} else {
		if(isInBoardBounds(p + dir) && isCellOccupied(p + dir) && vec4Equal(getCellColour(p), getCellColour(p + dir))) {
			if(h) h->push_back(vec2(p + dir)); else v->push_back(vec2(p + dir));
			recursiveCheck(p + dir, dir, h, v);
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------

void checkFruitColumn() {
	static vector<vec2> checkNext; // from previous call
	cout << "                 checkNExt: " << checkNext.size() << "    ";
	for(vector<vec2>::iterator toCheck = checkNext.begin(); toCheck != checkNext.end(); toCheck++) {
		printVec2(*toCheck);
		checkGroupedFruits(*toCheck);
	} cout << endl;
	checkNext.clear();

	sort(removedTiles.begin(), removedTiles.end(), sortByDecY());
	set<int> columnChecked;
	set<vector<vec2>::iterator> markDeletion;
	for(vector<vec2>::iterator hole = removedTiles.begin(); hole != removedTiles.end(); hole++) {
		if(columnChecked.insert(hole->x).second) { // successful insertion means this col hasn't been shifted down yet
			vec2 baseCellToCheck = vec2(hole->x, hole->y - 1);
			bool checkInNextIter = !isInBoardBounds(baseCellToCheck) || isCellOccupied(baseCellToCheck);
			if(!checkInNextIter && find(removedTiles.begin(), removedTiles.end(), baseCellToCheck)!= removedTiles.end()) { cout << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXxmissing"; checkInNextIter = true; }
			for(int y = hole->y ; y < BOARD_HEIGHT - 1; y++) {
				vec2 cellToBeDropped = vec2(hole->x, y + 1);
				vec2 cellToBeFilled = vec2(hole->x, y);
				if(isCellOccupied(cellToBeDropped)) {
					setCellOccupied(cellToBeDropped, false);
					setCellOccupied(cellToBeFilled, true);
					setCellColour(cellToBeFilled, getCellColour(cellToBeDropped));
					setCellColour(cellToBeDropped, cellFreeColour);
					if(checkInNextIter) checkNext.push_back(cellToBeFilled); // to next call
				}
			}
			markDeletion.insert(hole);
		}
	}
	for(set<vector<vec2>::iterator>::iterator it = markDeletion.begin(); it != markDeletion.end(); it++) removedTiles.erase(*it);
	glutTimerFunc(tileDropSpeed, tileDrop, TILE_COLUMN_CHECK);
}

void checkGroupedFruits(const vec2 &p) {
	vector<vec2> group, horzGroup, vertGroup;;
	recursiveCheck(p, vec2(0, 0), &horzGroup, &vertGroup);
	cout << p.x << "," << p.y << "         horzGroup.size(): " << horzGroup.size();
	for(int k = 0; k < (int)horzGroup.size(); k++) cout << "  " << horzGroup[k].x << "," << horzGroup[k].y;
	cout << "         vertGroup.size(): " << vertGroup.size();
	for(int k = 0; k < (int)vertGroup.size(); k++) cout << "  " << vertGroup[k].x << "," << vertGroup[k].y; cout << endl;
	horzGroup.size() >= MAX_FRUIT_GROUP ? group.swap(horzGroup) : group.swap(vertGroup);
	for(int k = 0; group.size() >= MAX_FRUIT_GROUP && k < MAX_FRUIT_GROUP; k++) {
		removedTiles.push_back(group[k]);
		removeTileFromBoard(group[k]);
	}
	numCheckFruitColumnCallbacks++;
	glutTimerFunc(tileDropSpeed, tileDrop, TILE_COLUMN_CHECK);
}

// Checks if the specified row (0 is the bottom 19 the top) is full
// If every cell in the row is occupied, it will clear that cell and everything above it will shift down one row
int checkFullRow(const vec2 &p) {
	int rowsRemoved = 0;
	int columnsHit = 0;
	for(int i = 0; i < BOARD_WIDTH && isCellOccupied(i, p.y); i++, columnsHit++);
	if(columnsHit == BOARD_WIDTH) {
		rowsRemoved++;
		for(int x = 0; x < BOARD_WIDTH; x++) {
			removeTileFromBoard(vec2(x, p.y));
			for(int y = p.y; y < BOARD_HEIGHT - 1; y++) {
				vec2 cellToBeDropped = vec2(x, y + 1);
				vec2 cellToBeFilled = vec2(x, y);
				if(isCellOccupied(cellToBeDropped)) {
					setCellOccupied(cellToBeDropped, false);
					setCellOccupied(cellToBeFilled, true);
					setCellColour(cellToBeFilled, getCellColour(cellToBeDropped));
					setCellColour(cellToBeDropped, cellFreeColour);
				}
			}
		}
		updateBoard();
	} 
	return rowsRemoved;
}

//-------------------------------------------------------------------------------------------------------------------

void tileDrop(int type) {
	TileInfo value = TILE_TICK;
	/*cout << "Y: " << type << endl;
	cout << "numDropTileCallbacks         : " << numDropTileCallbacks << endl;
	cout << "numFastDropTileCallbacks     : " << numFastDropTileCallbacks << endl;
	cout << "numCheckFruitColumnCallbacks : " << numCheckFruitColumnCallbacks << endl;*/
	switch(type) {
		case TILE_CREATE: // fall-through
		case TILE_TICK:
			if(numDropTileCallbacks > 1) { numDropTileCallbacks--; return; }
			if(numDropTileCallbacks == 1) {
				if(tileFreeToFall(currTilePos)) {
					currTilePos.y -= 1;
				} else {
					numFastDropTileCallbacks = 0;
					setTileColour(currTilePos);
					vector<vec2> lowestYCellsFirst; 
					for(int i = 0; i < 4; i++) lowestYCellsFirst.push_back(currTilePos + currTileOffset[i]);
					sort(lowestYCellsFirst.begin(), lowestYCellsFirst.end(), sortByIncY());
					int rowOffset = 0;
					for(int i = 0; i < 4; i++) {
						rowOffset+=checkFullRow(lowestYCellsFirst[i] - vec2(0, rowOffset));
						vector<vec2> horzGroup, vertGroup;
						recursiveCheck(lowestYCellsFirst[i], vec2(0, 0), &horzGroup, &vertGroup);
						if(horzGroup.size() >= MAX_FRUIT_GROUP)
							checkGroupedFruits(lowestYCellsFirst[i]);
						else if(i == 3)
							for(int k = 0; k < 4; k++) checkGroupedFruits(lowestYCellsFirst[k]);
					}
					newtile();
					value = TILE_CREATE;
				}
				updateBoard();
				updatetile();
				glutTimerFunc(tileDropSpeed, tileDrop, value);
			}
			return;
		case TILE_TICK_FAST:
			if(numFastDropTileCallbacks > 1) { numFastDropTileCallbacks--; return; }
			if(numFastDropTileCallbacks == 1) {
				if(tileFreeToFall(currTilePos)){
					currTilePos.y -= 1;
					updatetile();
					glutTimerFunc(TILE_DROP_SPEED_FAST, tileDrop, TILE_TICK_FAST);
				} else {
					numDropTileCallbacks++;
					glutTimerFunc(TILE_DROP_SPEED, tileDrop, TILE_TICK);
				}
			}
			return;
		case TILE_COLUMN_CHECK:
			if(numCheckFruitColumnCallbacks > 1) { numCheckFruitColumnCallbacks--; return; }
			if(numCheckFruitColumnCallbacks == 1)
				checkFruitColumn();
			return;
		default: cout << "WARNING: erroneous call to tileDrop" << endl; return;;
	}
}

//-------------------------------------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(xsize, ysize);
	glutInitWindowPosition(680, 178); // Center the game window (well, on a 1920x1080 display)
	glutCreateWindow("Fruit Tetris");
	glewInit();
	init();

	numDropTileCallbacks++;
	glutTimerFunc(tileDropSpeed, tileDrop, TILE_CREATE);

	// Callback functions
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(special);
	glutKeyboardFunc(keyboard);
	glutIdleFunc(idle);

	glutMainLoop(); // Start main loop
	return 0;
}

