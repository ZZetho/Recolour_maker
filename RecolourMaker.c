// TODO list:
// bug: ../../GameData/Tantares/parts/core_lk_v2/lk_v2_fuel_tank_s0_1.cfg model data reading does not work
// automatically calculate vectors
// show which areas on a part use what material using the svg's next to ModelData.txt
// re add lines between parent and child modules
// fix memory leaks

// DONE list:
// draw module tree vertically instead of horizontally
// find module names
// bug: modules not overwritten when selecting a new part
// find and display module types
// find corresponding modelData.txt
// parse corresponding modelData.txt
// display modelData.txt
// bug: fix converting from config file to model data file
// find the materials of every model a part uses
// reorganise code
// add windows for each part of the program
// add horizontal scrolling to function windows
// add scrollbar to function windows
// add horizontal scroll bars
// add model names to models
// add buttons that open / close the function windows
// only show important modules / materials
// fix memory leaks
// add a way to figure out what it will take to recolour a part
// redo the ui styling
// add icons to files depending on type
// bug: ../../GameData/TantaresLV/parts/BLOK_E/_AndromedaUS_VernierEngine_1.cfg not recognised as part config
// bug: ../../GameData/B9_Aerospace_Legacy/Parts/Utility_Landing_Gear_HDG/HDG1L.cfg not recognised as part cfg
// bug: ../../GameData/B9_Aerospace_Legacy/Parts/Aero_Wing_HW21/part.cfg model not found
// when parsing modules, more checks for being commented out
// redo recolouring types and program flow, eg: add textureSet type

// using clay for the ui
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_raylib.c"

#include <raylib.h>
#include <raymath.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define int int32_t

//▀█▀ █▄█ █▀█ █▀▀ █▀
//░█░ ░█░ █▀▀ ██▄ ▄█

typedef struct IntArray
{
	int * ints;
	int length;
}
IntArray;

// char array
typedef struct String
{
	char * chars;
	int length;
}
String;

typedef struct StringArray
{
	String * strings;
	int length;
}
StringArray;

typedef struct KeyValue
{
	String key;
	String value;
}
KeyValue;

typedef struct KeyValueArray
{
	KeyValue * keysValues;
	int length;
}
KeyValueArray;

typedef struct Module Module;	// Module is declared up here so Module array can have a pointer to it while Module can contain a Module array itself

typedef struct ModuleArray
{
	Module * modules;
	int length;
}
ModuleArray;

// the modules that part.cfg files are made of
struct Module
{
	ModuleArray children;
	Module * parent;
	String type;
	String * name;				// should point to the value of the property with the key 'name'
	KeyValueArray properties;
};

typedef struct ModelMaterial ModelMaterial;

// meshes that make up models
// raylib already took "Mesh"
typedef struct ModelMesh
{
	String name;
	int materialId;		// id of the material that is used by this mesh
	int id;				// unique id of this mesh, so that materials can find it
}
ModelMesh;

typedef struct MeshArray
{
	ModelMesh * meshes;
	int length;
}
MeshArray;

// the materials that are used on meshes
// raylib already took "Material"
struct ModelMaterial
{
	String materialName;
	String shaderName;
	KeyValueArray properties;
	IntArray meshIds;			// the ids of the meshes that use this material
	int id;						// unique id of this material, so that meshes can find it
};

typedef struct MaterialArray
{
	ModelMaterial * materials;
	int length;
}
MaterialArray;

// a model (.mu) used by parts
typedef struct MuModel
{
	MeshArray meshes;
	MaterialArray materials;
	String name;
}
MuModel;

typedef struct ModelArray
{
	MuModel * models;
	int length;
}
ModelArray;

// all of the information about a specific part
typedef struct PartInformation
{
	String configFilePath;			// file path to the parts config file
	String configFileContents;		// raw config file contents
	Module modules;					// parsed config file
	
	StringArray modelDataPaths;		// file paths to the parts models modelData.txt
	StringArray modelDataContents;	// raw modelData.txt contents
	ModelArray models;				// parsed modelData.txt's
}
PartInformation;

// texture with a name so it can be identified
typedef struct NamedTexture
{
	Texture texture;
	String name;
}
NamedTexture;

typedef struct NamedTextureArray
{
	NamedTexture * textures;
	int length;
}
NamedTextureArray;

// the textures used for embossed containers
typedef struct EmbossedTextures
{
	Texture * topLeft;
	Texture * topLeftInverse;
	Texture * top;
	Texture * topRight;
	Texture * topRightInverse;
	Texture * left;
	Texture * right;
	Texture * bottomLeft;
	Texture * bottomLeftInverse;
	Texture * bottom;
	Texture * bottomRight;
	Texture * bottomRightInverse;
}
EmbossedTextures;

// global variables
const int nullTerminator = 1;
const float floatImprecisionBuffer = 0.001f;
const NamedTextureArray * textureAtlas = NULL;
const EmbossedTextures * embossedTextures = NULL;

// global ui settings
const int padding = 5;
const int cornerRadius = 5;
const int fontSize = 15;
const Clay_Color textColour = (Clay_Color){ .r = 0, .b = 0, .g = 0, .a = 255};

// reusable 90's style embossed box
#define EMBOSSED_CONTAINER(containerWidth, containerHeight, ...)\
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = containerWidth, .height = containerHeight } } })\
	{																																												\
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })								\
		{																																											\
			/* draw the top border */																																				\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->topLeft } }) {}		\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->top } }) {}			\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->topRight } }) {}	\
		}																																											\
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })								\
		{	/* draw the left border */																																				\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_GROW() } }, .image = { .imageData = embossedTextures->left } }) {}			\
			/* this forces the right border to the right */																															\
			CLAY_AUTO_ID({ .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })\
			{																																										\
				/* add whatever was put as the ... paramater here */																												\
				__VA_ARGS__;																																						\
				/* draw the right border */																																			\
				CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_GROW() } }, .image = { .imageData = embossedTextures->right } }) {}		\
			}																																										\
		}																																											\
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })								\
		{																																											\
			/* draw the bottom border */																																			\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->bottomLeft } }) {}	\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->bottom } }) {}		\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->bottomRight } }) {}	\
		}																																											\
	}

// box with the embossing inverted, so it looks inset
#define EMBOSSED_CONTAINER_INVERSE(containerWidth, containerHeight, ...)\
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = containerWidth, .height = containerHeight } } })\
	{																																												\
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })								\
		{																																											\
			/* draw the top border */																																				\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->topLeftInverse } }) {}	\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->bottom } }) {}		\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->topRightInverse } }) {}	\
		}																																											\
		CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })								\
		{	/* draw the left border */																																				\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_GROW() } }, .image = { .imageData = embossedTextures->right } }) {}			\
			/* this forces the right border to the right */																															\
			CLAY_AUTO_ID({ .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })\
			{																																										\
				/* add whatever was put as the ... paramater here */																												\
				__VA_ARGS__;																																						\
				/* draw the right border */																																			\
				CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_GROW() } }, .image = { .imageData = embossedTextures->left } }) {}		\
			}																																										\
		}																																											\
		CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })								\
		{																																											\
			/* draw the bottom border */																																			\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->bottomLeftInverse } }) {}	\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->top } }) {}			\
			CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(2), .height = CLAY_SIZING_FIXED(2) } }, .image = { .imageData = embossedTextures->bottomRightInverse } }) {}	\
		}																																											\
	}
// gosh aren't these macro things fun! 😁️

//█▀▄▀█ █ █▀ █▀▀
//█░▀░█ █ ▄█ █▄▄

// an array of arrays of dynamically allocated chars from dynamically allocated Clay_String's
// these cant be freed normally in the middle if the frame because clay needs them to render the ui
// so they are collected here untill after rendering when they are freed
typedef struct DynamicClayStringCharsArray
{
	char ** charArrays;
	int length;
}
DynamicClayStringCharsArray;

// add the chars from dynamic clay strings here to have them freed at the end of a frame
DynamicClayStringCharsArray freeQueue = (DynamicClayStringCharsArray){ .charArrays = NULL, .length = 0 };

// add a char array to be freed at the and of the frame
void AddToFreeQueue(char * chars)
{
	freeQueue.length += 1;
	freeQueue.charArrays = realloc(freeQueue.charArrays, sizeof(char *) * freeQueue.length);
	freeQueue.charArrays[freeQueue.length - 1] = chars;
}

// free the chars
// call this after the rendering step
void FreeFreeQueue()
{
	for (int index = 0; index < freeQueue.length; index ++)
	{
		char * chars = freeQueue.charArrays[index];
		free(chars);
	}
	
	freeQueue.length = 0;
	freeQueue.charArrays = realloc(freeQueue.charArrays, sizeof(char *) * freeQueue.length);
}

// required by clay
void HandleClayErrors(Clay_ErrorData errorData)
{
	printf("%s", errorData.errorText.chars);
}

// returns the minimum of two ints
int MinInt(int a, int b)
{
	if (a < b)
	{
		return a;
	}
	return b;
}

int MaxInt(int a, int b)
{
	if (a > b)
	{
		return a;
	}
	return b;
}

float MinFloat(float a, float b)
{
	if (a < b)
	{
		return a;
	}
	return b;
}

float MaxFloat(float a, float b)
{
	if (a > b)
	{
		return a;
	}
	return b;
}

// finds the number of significant figures an int has
int SignificantFigures(int i)
{
	if (i < 0)
	{
		i *= -1;
	}

	int significantFigures = 1;

	while (i > 0)
	{
		significantFigures += 1;
		i /= 10;   
	}

	return significantFigures;
}

// for converts an int to a string
void IntToString(int i, String * output)
{
	bool negative = false;
	if (i < 0)
	{
		i *= -1;
		negative = true;
	}

	// allocate one char per sig fig, and an extra one for the
	// null terminator and the minus if negative
	// find how many significant figures the int has 
	int significantFigures = SignificantFigures(i);
	int outputLength = significantFigures + negative + nullTerminator;
	*output = (String){ .chars = realloc(output->chars, sizeof(char) * outputLength), .length = outputLength };

	//printf("output length: %d\n", outputLength);

	// for each sig fig,
	// divide it by ten per sig fig to the right to move it to the right most position
	// mod 10 to remove all other figures
	// add 48 to convert to a char
	for (int sigFigIndex = 0; sigFigIndex < significantFigures; sigFigIndex ++)
	{
		int figure = i / (int)pow(10, significantFigures - sigFigIndex - 1);
		figure %= 10;
		figure += 48;
		//printf("%d, %c, ", significantFigures - sigFigIndex - 1, figure);
		output->chars[sigFigIndex + negative] = figure;
	}

	if (negative == true)
	{
		//printf("negative\n");
		output->chars[0] = '-';
	}

	output->chars[outputLength - 1] = '\0';
	//printf("%s\n", output);
}

// converts a raylib colour to a clay colour
Clay_Color RaylibToClayColour(Color raylib)
{
	Clay_Color clay = (Clay_Color){ .r = raylib.r, .g = raylib.g, .b = raylib.b, .a = raylib.a };
	return clay;
}

// returns a unique number every time
int UniqueId()
{
	static int id = 0;
	id ++;
	return id;
}

//█▀ ▀█▀ █▀█ █ █▄░█ █▀▀   █▀▀ █░█ █▄░█ █▀▀ ▀█▀ █ █▀█ █▄░█ █▀
//▄█ ░█░ █▀▄ █ █░▀█ █▄█   █▀░ █▄█ █░▀█ █▄▄ ░█░ █ █▄█ █░▀█ ▄█

// finds the index of a char working backwards, or -1 if no char
int LastIndexOfChar(String string, int startingIndex, char target)
{
	if (startingIndex > string.length - 1 || startingIndex < 0)
	{
		//printf("startingIndex: %d out of range: 0-%d\n", startingIndex, string.length - 1);
		return -1;
	}

	for (int index = startingIndex; index >= 0; index --)
	{
		if (string.chars[index] == target)
		{
			return index;
		}
	}
	
	return -1;
}

// finds the index of the last apperance of a char, or -1 if no char
int IndexOfChar(String string, int startingIndex, char target)
{
	if (startingIndex > string.length - 1 || startingIndex < 0)
	{
		//printf("startingIndex: %d out of range: 0-%d\n", startingIndex, string.length - 1);
		return -1;
	}
	
	for (int index = startingIndex; index < string.length; index ++)
	{
		if (string.chars[index] == target)
		{
			return index;
		}
	}
	
	return -1;
}

bool IsWhitespace(char c)
{
	if (c == ' ' || c == '	' || c == '\r')
	{
		return true;
	}
	return false;
}

// returns the index of the next char that is not whitespace, or -1 if there are no more not whitespace chars
int IndexOfNextNotWhitespaceChar(String string, int startIndex)
{
	if (startIndex > string.length - 1 || startIndex < 0)
	{
		return -1;
	}
	
	for (int index = startIndex; index < string.length; index ++)
	{
		if (IsWhitespace(string.chars[index]) == false)
		{
			return index;
		}
	}
	
	return -1;
}

// print that indicates types of whitespace
void BetterPrint(String string)
{
	if (string.chars == NULL)
	{
		printf("NULL chars\n");
		return;
	}
	for (int index = 0; index < string.length; index ++)
	{
		if (string.chars[index] == '\0')
		{
			printf("<\\0>");
		}
		else if (string.chars[index] == ' ')
		{
			printf("<space>");
		}
		else if (string.chars[index] == '	')
		{
			printf("<tab>");
		}
		else if (string.chars[index] == '\n')
		{
			printf("<\\n>\n");
		}
		else if (string.chars[index] == '\r')
		{
			printf("<\\r>");
		}
		else
		{
			printf("%c", string.chars[index]);
		}
		//printf(" <- %d\n", index);
	}
	
	printf("\n");
}

//checks if a string includes a null terminator
bool HasNullTerminator(String string)
{
	if (string.chars == NULL)
	{
		//printf("uhoh\n");
		return false;
	}
	// check if the final char is '\0'
	if (string.chars[string.length - 1] == '\0')
	{
//		BetterPrint(string);
//		printf("has null terminator\n");
		
		return true;
	}
	
//	BetterPrint(string);
//	printf("has no null terminator\n");
	return false;
}

// resizes a string
void ResizeString(String * string, int length)
{
	// cant have a string with a length less than zero
	if (length < 0)
	{
		return;
	}
	
	int oldLength = string->length;
	
	// resize the string
	string->length = length;
	string->chars = realloc(string->chars, sizeof(char) * string->length);
	
	if (length == 0)
	{
		string->chars = NULL;
	}
	
	if (oldLength >= string->length)
	{
		// if the string has been shortened or stayed the same size
		// the next bit is unnessecary
		return;
	}
	
	// fill the rest of the string with \0
	for (int index = oldLength; index < string->length; index ++)
	{
		string->chars[index] = '\0';
	}
}

// adds a charchter to the end of a string
void AddCharToEndOfString(String * string, char newChar)
{
//	printf("old string: ");
//	BetterPrint(string);
	
	ResizeString(string, string->length + 1);
	string->chars[string->length - 1] = newChar;
	
//	printf("new string: ");
//	BetterPrint(string);
}

// garuntees a string has a null terminator
void GarunteeNullTerminator(String * string)
{
	// dont do anything if it already has one
	if (HasNullTerminator(*string) == true)
	{
		return;
	}
	
	//printf("adding null terminator\n");
	
	// add the null terminator if it has none
	AddCharToEndOfString(string, '\0');
	
	return;
}

// removes \0 from a string if it has it
void GarunteeNoNullTerminator(String * string)
{
	if (HasNullTerminator(*string) == false)
	{
		return;
	}
	
	// if a string ends in \0, remove it
	ResizeString(string, string->length - 1);
	return;
}

// converts a clay string to one of my strings
String ClayToMyString(Clay_String clayString)
{
	String string = (String){ .length = clayString.length, .chars = malloc(sizeof(char) * clayString.length) };
	memcpy(string.chars, clayString.chars, sizeof(char) * clayString.length);
	// clay strings are not garunteed to contain a null terminator
	GarunteeNullTerminator(&string);
	return string;
}

// converts one of my strings to a clay string
// the chars are automatically freed at the end of the frame
// string.chars cannot be freed immediatley after use if the output is being put into CLAY_TEXT, as clay needs to use the chars later in the rendering step
Clay_String MyToClayString(String myString)
{
	// do not include the null terminator (if it exists)
	int length = myString.length;
	if (HasNullTerminator(myString) == true)
	{
		length -= 1;
	}
	
	// create a palce to store the chars as we cant memcpy to Clay_String.chars as it is const
	char * chars = malloc(sizeof(char) * length);
	memcpy(chars, myString.chars, sizeof(char) * length);
	// point to the new char array
	Clay_String string = (Clay_String){ .isStaticallyAllocated = false, .length = length, .chars = chars };
	// queue the cahrs for freeing at the end of the frame
	AddToFreeQueue(chars);
	return string;
}

// removes whitespace from the start and end of a string
void TrimWhitespace(String * string)
{
	GarunteeNullTerminator(string);
	
	while (IsWhitespace(string->chars[0]) == true && string->length > 0)
	{
		// remove the first char
		string->length -= 1;
		// copy the rest of the string over the first char
		memmove(string->chars, &string->chars[1], sizeof(char) * string->length);
		string->chars = realloc(string->chars, sizeof(char) * string->length);
	}
	
	//printf("final char: ");
	//BetterPrint((String){ .length = 1, .chars = &string->chars[string->length - 1 - nullTerminator] });
	
	while(IsWhitespace(string->chars[string->length - 1 - nullTerminator]) == true && string->length > 0)
	{
		//remove the last (not \0) char
		ResizeString(string, string->length - 1);
		// replace the last char with \0
		string->chars[string->length - 1] = '\0';
	}
}

// removes whitespace from a string
void RemoveWhiteSpace(String * string)
{
	for (int index = 0; index < string->length; index ++)
	{
		if (IsWhitespace(string->chars[index]) == true)
		{
			// move the string after this point on top of the whitespace
			// V......V.......	7
			// .string.string\0	15
			int charsAfterWhitespace = string->length - index - 1;
			memmove(&string->chars[index], &string->chars[index + 1], sizeof(char) * charsAfterWhitespace);
			string->length -= 1;
			index -= 1;
		}
	}
}

// deletes whitespace and \n
void RemoveWhitespaceAndNewlines(String * string)
{
	for (int index = 0; index < string->length; index ++)
	{
//		printf("char: ");
//		BetterPrint((String){ .chars = &string->chars[index], .length = 1});
		
		if (IsWhitespace(string->chars[index]) == true || string->chars[index] == '\n')
		{
			// printf("is whitespace / \\n\n");
			// move the string after this point on top of the whitespace
			// V......V.......	7
			// .string.string\0	15
			int charsAfterWhitespace = string->length - index - 1;
			memmove(&string->chars[index], &string->chars[index + 1], sizeof(char) * charsAfterWhitespace);
			string->length -= 1;
			index -= 1;
		}
		// printf("is not whitespace / \\n\n");
	}
}

// check if two strings are equal
bool StringsEqual(String a, String b)
{
	if (a.length != b.length)
	{
		//printf("length: %d != %d\n", a.length, b.length);
		return false;
	}
	
	for (int charIndex = 0; charIndex < a.length && charIndex < b.length; charIndex ++)
	{
		if (a.chars[charIndex] != b.chars[charIndex])
		{
			//printf("chars: %c != %c\n", a.chars[charIndex], b.chars[charIndex]);
			return false;
		}
	}
	
	return true;
}

// checks if a string equals any of those in an array
bool StringEqualToAny(String a, StringArray strings)
{
	for (int index = 0; index < strings.length; index ++)
	{
		if (StringsEqual(a, strings.strings[index]) == true)
		{
			return true;
		}
	}
	
	return false;
}

bool KeyValuesEqual(KeyValue a, KeyValue b)
{
	if (StringsEqual(a.key, b.key) == true && StringsEqual(a.value, b.value) == true)
	{
		return true;
	}
	
	return false;
}

bool KeyValueEqualToAny(KeyValue a, KeyValueArray keysValues)
{
	for (int index = 0; index < keysValues.length; index ++)
	{
		if (KeyValuesEqual(a, keysValues.keysValues[index]) == true)
		{
			return true;
		}
	}
	
	return false;
}

// extracts a string from a larger string
void ExtractString(String extractee, String * extracted, int startIndex, int endIndex)
{
	// clear extracted
	ResizeString(extracted, 0);
	
	if (startIndex < 0)
	{
		//printf("ERROR: start index: %d was less than zero\n", startIndex);
		return;
	}
	
	if (endIndex <= startIndex)
	{
		//printf("ERROR: end index: %d was less than or equal to start index: %d\n", endIndex, startIndex);
		return;
	}
	
	if (startIndex > extractee.length - 1)
	{
		//printf("ERROR: start index: %d was after extractee end\n", startIndex);
		return;
	}
	
	if (endIndex > extractee.length - 1)
	{
		//printf("ERROR: end index: %d was after extractee end\n", endIndex);
		return;
	}
	
	//	   5	 11
	//	 V	 V
	//extractee.string
	
	// resize extracted to put the extracted chars into
	int extractedLength = endIndex - startIndex + 1;
	ResizeString(extracted, extractedLength);
	
	// copy into the extracted string from the extractee
	memcpy(extracted->chars, &extractee.chars[startIndex], sizeof(char) * extracted->length);
}

// adds the second string to the end of the first one
void ConcatenateStrings(String stringOne, String stringTwo, String * output)
{
	//BetterPrint(stringOne);
	//BetterPrint(stringTwo);
	//printf("being combined to make:\n");
	
	// egg\0			(length 4)
	// cheese\0			(length 7)
	// -> eggcheese\0	(length 10)
	
	// strings may not have a null terminator
	
	int stringTwoAddIndex = stringOne.length;
	
	// if string one has a null terminator, add string two on top of it to get rid of it
	if (stringOne.length >= 1 && stringOne.chars[stringOne.length - 1] == '\0')
	{
		stringTwoAddIndex -= 1;
	}
	
	int outputLength = stringTwoAddIndex + stringTwo.length;
	ResizeString(output, outputLength);
	
	memcpy(output->chars, stringOne.chars, sizeof(char) * stringOne.length);
	memcpy(&output->chars[stringTwoAddIndex], stringTwo.chars, sizeof(char) * stringTwo.length);
	
	//BetterPrint(*output);
}

// adds a string to the end of another
void AddToString(String * addee, String added)
{
	int addIndex = addee->length;
	
	// if addee has a null terminator, add added on top of it to get rid of it
	if (addee->length >= 1 && addee->chars[addee->length - 1] == '\0')
	{
		addIndex -= 1;
	}
	
	//printf("addIndex: %d\n", addIndex);
	
	int newLength = addIndex + added.length;
	
	//printf("newLength: %d\n", newLength);
	
	ResizeString(addee, newLength);
	
	memcpy(&addee->chars[addIndex], added.chars, sizeof(char) * added.length);
	
	//printf("added to string:\n%s\n", addee->chars);
}

// checks if a string starts with another string
bool StringStartsWith(String string, String subString)
{
	for (int subStringIndex = 0; subStringIndex < subString.length; subStringIndex ++)
	{
		//printf("string char: %c\n", string.chars[subStringIndex]);
		//printf("sub string char: %c\n", subString.chars[subStringIndex]);
		
		// check if each char matches
		if (string.chars[subStringIndex] != subString.chars[subStringIndex])
		{
			return false;
		}
	}
	
	return true;
}

// returns the index of a substring within a larger string
// or -1 if the substring is not found
int IndexOfSubstring(String string, int startIndex, String subString)
{
	//printf("String length: %d\n", string.length);
	//printf("substring length: %d\n", subString.length);
	
//	printf("string: ");
//	BetterPrint(string);

	if (subString.length > string.length)
	{
		return -1;
	}
	
	// remove the null terminator from the substring
	GarunteeNoNullTerminator(&subString);
	
	// substring: egg			length: 3
	// string: ham_and_eggs\0	length: 13
	//				   ^
	//			dont search past here (index: 10)
	
	int finalSearchIndex = string.length - subString.length;
	
	//printf("final search index: %d\n", finalSearchIndex);
	
	for (int searchIndex = startIndex; searchIndex <= finalSearchIndex; searchIndex ++)
	{
		String stringFromSearchIndex = (String){ .length = string.length - searchIndex, .chars = &string.chars[searchIndex] };
//		printf("search index: %d\n", searchIndex);
//		printf("string length: %d\n", string.length);
//		printf("string from search index length: %d\n", stringFromSearchIndex.length);
//		BetterPrint(stringFromSearchIndex);
		
		if (StringStartsWith(stringFromSearchIndex, subString) == true)
		{
			//BetterPrint(subString);
			//printf("was found at: %d\n", searchIndex);
			
			return searchIndex;
		}
	}
	
//	BetterPrint(subString);
//	printf("was not found in: ");
//	BetterPrint(string);
	
	// if the substring was not found
	return -1;
}

// returns a string that is no more than length long
void ClampStringLength(String string, String * newString, int maxLength)
{
	ResizeString(newString, string.length);
	
	if (newString->length < 1)
	{
		return;
	}
	
	// copy the string into the new one
	memcpy(newString->chars, string.chars, sizeof(char) * string.length);
	
	// if the string is short enough or maxLength is -1 (ignore max length)
	// do not shorten
	if (string.length < maxLength || maxLength == -1)
	{
		return;
	}
	
	// shorten the new string
	ResizeString(newString, maxLength - 3);
	
	// add ... the end of the string
	AddCharToEndOfString(newString, '.');
	AddCharToEndOfString(newString, '.');
	AddCharToEndOfString(newString, '.');
	GarunteeNullTerminator(newString);
	
	//printf("clamped string: ");
	//BetterPrint(*newString);
	
	return;
}

// returns the index of a key in a key value array
int IndexOfKey(KeyValueArray array, int startIndex, String key)
{
	if (startIndex < 0 || startIndex > array.length - 1)
	{
		return -1;
	}

	//printf("keys and values: %d\n", array.length);
	for (int index = startIndex; index < array.length; index ++)
	{
		if (StringsEqual(array.keysValues[index].key, key) == true)
		{
			return index;
		}
	}
	
	return -1;
}

// finds the value associated with a key
void ValueOfKey(KeyValueArray array, String key, String * output)
{
	*output = (String){ .chars = NULL, .length = 0 };
	int keyIndex = IndexOfKey(array, 0, key);
	
	if (keyIndex == -1)
	{
		return;
	}
	
	String value = array.keysValues[keyIndex].value;
	
	output->length = value.length;
	ResizeString(output, output->length);
	memcpy(output->chars, value.chars, output->length);
}

void FreeStringArray(StringArray * stringArray)
{
	for (int index = 0; index < stringArray->length; index ++)
	{
		String string = stringArray->strings[index];
		free(string.chars);
	}
	
	free(stringArray->strings);
}

//█▀▀ █ █░░ █▀▀   █░█ ▄▀█ █▄░█ █▀▄ █░░ █ █▄░█ █▀▀
//█▀░ █ █▄▄ ██▄   █▀█ █▀█ █░▀█ █▄▀ █▄▄ █ █░▀█ █▄█

// reads a whole file as a string
void ReadFile(String filePath, String * output)
{
	// try and open the file
	FILE * file = fopen(filePath.chars, "r");
	
	if (file == NULL)
	{
		int err = errno;
		printf("ERROR: Could not open: %s because: %s\n", filePath.chars, strerror(err));
		
		return;
	}
	
	printf("reading file: %s\n", filePath.chars);
	
	// read the data from the config file
	const size_t bufferSize = 100;
	String buffer = (String){ .chars = NULL, .length = 0 };
	ResizeString(&buffer, bufferSize);
	
	int writeIndex = 0;			// the index to write to in the otuput from the buffer
	ResizeString(output, 0);	// clear the otuput
	
	size_t readObjectCount = 0;	// how many chars have been read this read cycle
	
	while (true)
	{
		// read the next 100 chars
		readObjectCount = fread(buffer.chars, sizeof(char), buffer.length, file);
		
		// add the number read chars
		int configFileSize = output->length + readObjectCount;
		ResizeString(output, configFileSize);
		
		// copy the read chars from the buffer to the config file string
		memcpy(&output->chars[writeIndex], buffer.chars, sizeof(char) * readObjectCount);
		
		// advance the write index
		writeIndex += readObjectCount;
		
		if (feof(file) == true)
		{
			printf("reached end of file\n");
			break;
		}
		
		if (readObjectCount != bufferSize)
		{
			printf("ERROR: tried to read %lu chars, but read %lu instead\n", bufferSize, readObjectCount);
			break;
		}
	}
	
	GarunteeNullTerminator(output);
	
	// the whole config file has been read, close it
	fclose(file);
	free(buffer.chars);
}

// read from an array of file paths
void ReadFiles(StringArray filePaths, StringArray * output)
{
	*output = (StringArray){ .strings = NULL, .length = 0 };
	
	for (int index = 0; index < filePaths.length; index ++)
	{
		String fileContents = (String){ .chars = NULL, .length = 0 };
		ReadFile(filePaths.strings[index], &fileContents);
		
		output->length += 1;
		output->strings = realloc(output->strings, sizeof(String) * output->length);
		output->strings[output->length - 1] = fileContents;
	}
}

//█▀▀ █▀█ █▄░█ █▀▀ █ █▀▀   █▀▀ █ █░░ █▀▀   █▀█ ▄▀█ █▀█ █▀ █ █▄░█ █▀▀
//█▄▄ █▄█ █░▀█ █▀░ █ █▄█   █▀░ █ █▄▄ ██▄   █▀▀ █▀█ █▀▄ ▄█ █ █░▀█ █▄█

// removes :NEEDS[xyz]
void RemoveNeeds(String * string)
{
	int colonIndex = IndexOfChar(*string, 0, ':');
	
	// if not colon is found
	if (colonIndex == -1)
	{
		return;
	}
	
	// if a colon is found, it is assumed to be the start of a :NEEDS
	// :NEEDS ends with a ]
	int needsEndIndex = IndexOfChar(*string, colonIndex, ']') + 1;
	int needsLength = needsEndIndex - colonIndex;
	int charsAfterNeeds = string->length - needsEndIndex + 1;
	
	printf("charsAfterNeeds: %d\n", charsAfterNeeds);
	
	// move the rest of the string over the :NEEDS
	memmove(&string->chars[colonIndex], &string->chars[colonIndex + needsLength], sizeof(char) * charsAfterNeeds);
	int newLength = string->length - needsLength;
	ResizeString(string, newLength);
}

// removes comments
void RemoveComments(String * string)
{
	// comments start with a // and end with a \n
	// if a comment is found remove everything from the // to before the \n
	int commentIndex = 0;
	int newLineIndex = 0;
	
	while (true)
	{
		// find the next // and \n
		commentIndex = IndexOfSubstring(*string, commentIndex, (String){ .chars = "//", .length = 2 });
		
		// if there are no more comments
		if (commentIndex == -1)
		{
			return;
		}
		
		// a comment has been found, find the next \n
		newLineIndex = IndexOfChar(*string, commentIndex, '\n');
		
		// no more new line are found, the rest of the string is a comment
		if (newLineIndex == -1)
		{
			// ........V........ 8
			// string..//comment 8
			//
			// cut off the string at the comment
			ResizeString(string, commentIndex);
			GarunteeNullTerminator(string);
			return;
		}
		
		// a comment and subsequent newline has been found, remove everything in between
		// copy all of the string from the /n to where the // is
		int commentLength = newLineIndex - commentIndex;
		int charsAfterComment = string->length - newLineIndex;
		int newLength = string->length - commentLength;
		memmove(&string->chars[commentIndex], &string->chars[newLineIndex], sizeof(char) * charsAfterComment);
		ResizeString(string, newLength);
	}
}

// check if a config file is a .cfg for a part
bool IsPartConfig(String configFile)
{
	// look for "PART"
	String part = (String){ .chars = "PART", .length = 4 };
	int partIndex = IndexOfSubstring(configFile, 0, part);
	
	if (partIndex == -1)
	{
		printf("no \"PART\", not a config file\n");
		return false;
	}
	
	// find the nearest { and }
	int openBracketIndex = IndexOfChar(configFile, partIndex, '{');
	int closeBracketIndex = IndexOfChar(configFile, partIndex, '}');
	
	// if the next { is farther than the next }
	if (closeBracketIndex < openBracketIndex)
	{
		printf("next bracket after PART was }, not a config file\n");
		return false;
	}
	
	// get everything from the PART to the {
	String partToBracket = (String){ .chars = NULL, .length = 0 };
	ExtractString(configFile, &partToBracket, partIndex, openBracketIndex);
	// remove whitespace and \n
	RemoveWhitespaceAndNewlines(&partToBracket);
	GarunteeNullTerminator(&partToBracket);
	RemoveNeeds(&partToBracket);
	
//	printf("partToBracket:\n");
//	BetterPrint(partToBracket);
	
	if (StringsEqual(partToBracket, (String){ .chars = "PART{", .length = 6 }) == true)
	{
		printf("is a part config file\n");
		return true;
	}
	
	printf("%s does not = \"PART{\", not a config file\n", partToBracket.chars);
	return false;
}

void ParseModuleProperties(String configFile, int startIndex, int endIndex, Module * module)
{
	// between the start and end, parse the properties and values of each line
	// first off, find the next \n, everything in between it and the last \n is a line
	// should be in the format: property = value
	// look for the =
	// the property will be everything between the = and start of the line (\n)
	// value is everything between the = and the \n or //
	// value may include whitespace and single /'s
	// property probably cant have whitespace? whatevs
	// trim whitespace from the ends of property and value
	
	// once all properties have been parsed, look for the 'name' propety
	// and set it as this modules name
	
	int lineStart = startIndex;
	int lineEnd = startIndex;
	
	while (lineEnd < endIndex)
	{
		// the start and end of the last line
		lineStart = lineEnd;
		lineEnd = IndexOfChar(configFile, lineStart + 1, '\n');
		
		if (lineEnd == -1)
		{
			//hit the end of the file
			return;
		}
		// ......V V  6,7
		// string\n\n
		
		// extract the line from the configFile, excluding the \n's
		String line = (String){ .chars = NULL, .length = 0 };
		ExtractString(configFile, &line, lineStart + 1, lineEnd - 1);
		
		// look for the =
		int equalsIndex = IndexOfChar(line, 0, '=');
		
		if (equalsIndex == -1 )
		{
			// no = found on this line, not a property
			free(line.chars);
			continue;
		}
		
		// everything from \n to =
		String preEquals = { .length = 0, .chars = NULL };
		ExtractString(line, &preEquals, 0, equalsIndex - 1);
		
		// 0		  12
		// V		   V
		//\n....cheese.=.egg...\n
		
		//everything from = onwards
		String postEquals = { .chars = NULL, .length = 0 };
		ExtractString(line, &postEquals, equalsIndex + 1, line.length - 1);
		// TODO: free pre / post equals when a new file is parsed
		//			  12	   20
		//			 V	   V
		//\n....cheese.=.egg...\n
		
		GarunteeNullTerminator(&preEquals);
		GarunteeNullTerminator(&postEquals);
		
		TrimWhitespace(&preEquals);
		TrimWhitespace(&postEquals);
		
		printf("%s = %s\n", preEquals.chars, postEquals.chars);
		//printf("line end: %d\n", lineEnd);
		
		// convert the strings to a key value pair and add them to the module
		module->properties.length += 1;
		module->properties.keysValues = realloc(module->properties.keysValues, sizeof(KeyValue) * module->properties.length);
		
		module->properties.keysValues[module->properties.length - 1].key = (String){ .length = preEquals.length, .chars = malloc(sizeof(char) * preEquals.length)};
		module->properties.keysValues[module->properties.length - 1].value = (String){ .length = postEquals.length, .chars = malloc(sizeof(char) * postEquals.length)};
		
		memcpy(module->properties.keysValues[module->properties.length - 1].key.chars, preEquals.chars, sizeof(char) * preEquals.length);
		memcpy(module->properties.keysValues[module->properties.length - 1].value.chars, postEquals.chars, sizeof(char) * postEquals.length);
		
		free(line.chars);
	}
	
	// see if any of the keys are "name"
	// set the modules name if so
	String name = (String){ .chars = "name", .length = 5 };
	int nameIndex = IndexOfKey(module->properties, 0, name);
	
	if (nameIndex != -1)
	{
		module->name = &module->properties.keysValues[nameIndex].value;
	}
}

// the rules for what strings are valid module types
bool IsValidType(String type)
{
	//printf("type candidate: %s\n", type.chars);

	if (type.length <= 0)
	{
		return false;
	}
	if (type.length == 1 && type.chars[0] == '\0')
	{
		return false;
	}
	
	return true;
}

// looks for the type of the module
void ParseModuleType(String configFile, int bracketIndex, Module * module)
{
	// search the line that the { is on and the previous line
	int lineStart = LastIndexOfChar(configFile, bracketIndex, '\n');
	
	if (lineStart == -1)
	{
		// means there were no \n's berfore this bracket
		// for a normal part file this should be impossible
		printf("ERROR: could not find \\n before {\n");
		return;
	}
	
	// see if there is a type between /n and {
	// V		V
	//\n..TYPE..{
	String type = (String){ .chars = NULL, .length = 0 };
	ExtractString(configFile, &type, lineStart + 1, bracketIndex - 1);
	TrimWhitespace(&type);
	
	if (IsValidType(type) == true)
	{
		// a type has been found
		// set the found type as the modules type
		module->type = type;
		return;
	}
	
	// look for a type on the previous line
	int previousLineStart = LastIndexOfChar(configFile, lineStart - 1, '\n');
	int previousLineComment = IndexOfSubstring(configFile, previousLineStart, (String){ .chars = "//", .length = 2 });
	
	if (previousLineComment == -1)
	{
		previousLineComment = INT32_MAX;
	}
	
	int previousLineEnd = MinInt(lineStart, previousLineComment);
	
//	printf("prev line start: %d\n", previousLineStart);
//	printf("prev line comment: %d\n", previousLineComment);
//	printf("prev line end: %d\n", previousLineEnd);
	
	// see if there is a type on the previous line
	// V		  V
	//\n...TYPE...\n
	ExtractString(configFile, &type, previousLineStart + 1, previousLineEnd - 1);
	TrimWhitespace(&type);
	
//	printf("type candidate: ");
//	BetterPrint(type);
	
	if (IsValidType(type) == true)
	{
		// a type has been found
		// set the found type as the modules type
		module->type = type;
		return;
	}
	
	printf("could not find valid type\n");
}

// parses a module from { to }
// returns the index of the end of the module
int ParseModules(String configFile, int moduleStart, Module * currentModule)
{
	// find the type of this module
	ParseModuleType(configFile, moduleStart, currentModule);
	
	//printf("module type: ");
	//BetterPrint(currentModule->type);
	
	// assume moduleStart is at an {
	int searchIndex = moduleStart;
	
	while (true)
	{
		// find the next { or }
		//printf("searching for brackets from: %d\n", searchIndex + 1);
		
		int nextCloseBracket = IndexOfChar(configFile, searchIndex + 1, '}');
		int nextOpenBracket = IndexOfChar(configFile, searchIndex + 1, '{');
		
		bool closeBracketFound = true;
		bool openBracketFound = true;
		
		// if a } was not found, this should not happen
		if (nextCloseBracket == -1)
		{
			printf("ERROR: no closing brackets found, searching from: %d\n", searchIndex + 1);
			nextCloseBracket = INT32_MAX;
			closeBracketFound = false;
		}
		
		// if an { was not found, this should be the end of the top-level part module
		if (nextOpenBracket == -1)
		{
			nextOpenBracket = INT32_MAX;
			openBracketFound = false;
		}
		
		//printf("next open bracket { found at: %d\n", nextOpenBracket);
		//printf("next close bracket } found at: %d\n", nextCloseBracket);
		
		// if a { is next, parse the associated module
		if (openBracketFound == true && nextOpenBracket < nextCloseBracket)
		{
			// parse the properties of this module from where searching started to the found {
			ParseModuleProperties(configFile, searchIndex, nextOpenBracket, currentModule);
			
			// add a new child module to the current module
			ModuleArray * children = &currentModule->children;
			children->length += 1;
			children->modules = realloc(children->modules, sizeof(Module) * children->length);
			children->modules[children->length - 1] = (Module){ .type = (String){ 0 }, .name = NULL, .children = (ModuleArray){ .modules = NULL, .length = 0 }, .parent = currentModule};
			children->modules[children->length - 1].name = &(String){ .length = 0, .chars = NULL};
			
			// parse the new module and keep searching for the end of this one from the end of that module
			searchIndex = ParseModules(configFile, nextOpenBracket, &children->modules[children->length - 1]);
		}
		else if (closeBracketFound == true) // no open bracket was found or next close bracket was closer than next open bracket
		{
			// the next closest bracket is a }, this is the end of the module
			// parse the modules properties from where searching started to the found }
			ParseModuleProperties(configFile, searchIndex, nextCloseBracket, currentModule);
			
			//printf("module name: %s\n", currentModule->name->chars);
			
			return nextCloseBracket;
		}
		else
		{
			// no closing bracket was found
			// this should not happen (uh oh)
			return searchIndex + 1;
		}
	}
}

// free the strings that make up the keys and value of the array, and then the array itself
void FreeKeyValueArray(KeyValueArray * keysValues)
{
	for (int index = 0; index < keysValues->length; index ++)
	{
		KeyValue keyValue = keysValues->keysValues[index];
		
		free(keyValue.key.chars);
		free(keyValue.value.chars);
	}
	
	free(keysValues->keysValues);
}

void FreeModule(Module * module)
{
	// free each of the children of this module's children
	for (int index = 0; index < module->children.length; index ++)
	{
		FreeModule(&module->children.modules[index]);
	}
	// free the children array
	free(module->children.modules);
	
	free(module->type.chars);
	FreeKeyValueArray(&module->properties);
	
}

void BlankModule(Module * module)
{
	FreeModule(module);
	
	module->children = (ModuleArray){ .modules = NULL, .length = 0 };
	module->parent = NULL;
	module->type = (String){ .chars = NULL, .length = 0 };
	module->name = NULL;
	module->properties = (KeyValueArray){ .length = 0, .keysValues = NULL };
}

void BlankPart(PartInformation * part)
{
	free(part->configFilePath.chars);
	free(part->configFileContents.chars);
	
	part->configFilePath = (String){ .chars = NULL, .length = 0 };
	part->configFileContents = (String){ .chars = NULL, .length = 0 };
	
	BlankModule(&part->modules);
}

// loads a .cfg file into part information if it is a config file for a part
// returns true / false if it succedded or not
bool LoadConfigFile(PartInformation * part, String configFilePath)
{
	// open the .cfg (.txt) file at currentDirectory/fileName and turn it into a string
	// only if it is the config file for a part
	// combine the filename and current directory to get the full file path
	
	// try and read the config file
	String configFileContents = (String){ .chars = NULL, .length = 0 };
	ReadFile(configFilePath, &configFileContents);
	
	// if the config is not for a part, throw it out
	if (IsPartConfig(configFileContents) == false)
	{
		free(configFileContents.chars);
		return false;
	}
	
	// if the file is a part config file, clear the part object, ready for new data to be added to it
	BlankPart(part);
	
	part->configFilePath = configFilePath;
	part->configFileContents = configFileContents;
	
	GarunteeNullTerminator(&part->configFilePath);
	GarunteeNullTerminator(&part->configFileContents);
	
	// get rid of comments
	RemoveComments(&part->configFileContents);
	
	printf("opened config file: %s\n", part->configFilePath.chars);
	printf("contents:\n%s\n", part->configFileContents.chars);
	
	// index of the first {
	// this is where modules start being defined
	int modulesStartIndex = IndexOfChar(part->configFileContents, 0, '{');
	
	// parse the config string into a tree of modules
	ParseModules(part->configFileContents, modulesStartIndex, &part->modules);
	
	return true;
}

//█▀▄▀█ █▀█ █▀▄ █▀▀ █░░ █▀▄ ▄▀█ ▀█▀ ▄▀█   █▀█ ▄▀█ █▀█ █▀ █ █▄░█ █▀▀
//█░▀░█ █▄█ █▄▀ ██▄ █▄▄ █▄▀ █▀█ ░█░ █▀█   █▀▀ █▀█ █▀▄ ▄█ █ █░▀█ █▄█

// uses a path to a config file to find the path to the parts model data file
void ModelPathToModelDataPath(String modelPath, String * output)
{
	// convert the file path to the model to a file path to model data
	// from: 	ModName/modelName
	// to:		../../exportedUVMaps/ModName/modelName/ModelData.txt
	
	// initialise the model data path as the path to the model model
	*output = (String){ .chars = NULL, .length = 0 };
	ResizeString(output, modelPath.length);
	memcpy(output->chars, modelPath.chars, sizeof(char) * modelPath.length);
	
	// add "../../exportedUVMaps/" to the start
	int exportedUVMapsLength = 21;
	ResizeString(output, output->length + exportedUVMapsLength);
	// move the string over to allow adding exportedUVMaps
	memcpy(&output->chars[exportedUVMapsLength], output->chars, sizeof(char) * modelPath.length);
	// add exportedUVMaps
	memcpy(output->chars, "../../exportedUVMaps/", sizeof(char) * exportedUVMapsLength);
	
	// add "/ModelData.txt" to the end
	int modelDataLength = 14;
	int modelDataPathLength = output->length;
	ResizeString(output, output->length + modelDataLength);
	memcpy(&output->chars[modelDataPathLength - 1], "/ModelData.txt", sizeof(char) * modelDataLength);
	
	printf("converted config file path to: ");
	BetterPrint(*output);
	//printf("length: %d\n", modelDataPath.length);
}

// converts an array of model paths to modelData.txt paths
void ModelPathsToModelDataPaths(StringArray modelPaths, StringArray * output)
{
	*output = (StringArray){ .strings = NULL, .length = 0 };
	
	for (int index = 0; index < modelPaths.length; index ++)
	{
		String modelDataPath = (String){ .chars = NULL, .length = 0 };
		ModelPathToModelDataPath(modelPaths.strings[index], &modelDataPath);
		
		output->length += 1;
		output->strings = realloc(output->strings, sizeof(String) * output->length);
		output->strings[output->length - 1] = modelDataPath;
	}
}

// returns the name of the material that the mesh starting (has the start of its first minus row) at meshStartIndex uses
String FindMaterialName(String modelData, int meshStartIndex)
{
	// material name is on the fifth line
	// find the index of the start of the fifth line
	int materialNameLine = IndexOfChar(modelData, IndexOfChar(modelData, IndexOfChar(modelData, IndexOfChar(modelData, IndexOfChar(modelData, meshStartIndex, '\n') + 1, '\n') + 1, '\n') + 1, '\n') + 1, '\n');
	// material name starts at the : and ends at the end of the line
	int materialNameStart = IndexOfChar(modelData, materialNameLine, ':') + 1;
	int materialNameEnd = IndexOfChar(modelData, materialNameStart, '\n');
	
	int materialNameLength = materialNameEnd - materialNameStart;
	String materialName = (String){ 0 };
	ResizeString(&materialName, materialNameLength);
	memcpy(materialName.chars, &modelData.chars[materialNameStart], sizeof(char) * materialNameLength);
	TrimWhitespace(&materialName);
	
	//BetterPrint(materialName);
	
	return materialName;
}

// returns the name of the mesh that starts (has the start of its first minus row) at meshStartIndex
String FindMeshName(String modelData, int meshStartIndex)
{
	// mesh name is on the first line
	// find the index of the start of the first line
	int meshNameLine = IndexOfChar(modelData, meshStartIndex, '\n');
	// material name starts at the : and ends at the end of the line
	int meshNameStart = IndexOfChar(modelData, meshNameLine, ':') + 1;
	int meshNameEnd = IndexOfChar(modelData, meshNameStart, '\n');
	
	int meshNameLength = meshNameEnd - meshNameStart;
	String meshName = (String){ 0 };
	ResizeString(&meshName, meshNameLength);
	memcpy(meshName.chars, &modelData.chars[meshNameStart], sizeof(char) * meshNameLength);
	TrimWhitespace(&meshName);
	
	printf("mesh name: ");
	BetterPrint(meshName);
	
	return meshName;
}

// checks if the model already has a material with that name, returns its index, or -1 if none were found
int HasMaterialWithName(MuModel model, String materialName)
{
	MaterialArray materials = model.materials;
	
	// if the model has no materials it cannot have a material called materialName
	if (materials.length <= 0)
	{
		return -1;
	}
	
	// check the name against the names of the materials that the model has
	for (int index = 0; index < materials.length; index ++)
	{
		if (StringsEqual(materials.materials[index].materialName, materialName) == true)
		{
			return index;
		}
	}
	
	return -1;
}

// removes UnityEngine.Texture2D from the end of a string
void TrimUnityEngineTexture2D(String * string)
{
	// check if the string ends in " (UnityEngine.Texture2D)"
	String unityEngineTexture2D = (String){ .chars = " (UnityEngine.Texture2D)", .length = 25 };
	
	int endIndex = string->length - 1;
	int startIndex = endIndex - unityEngineTexture2D.length + 1;
	
	if (startIndex < 0)
	{
		return;
	}
	
	String stringEnd = (String){ .chars = NULL, .length = 0 };
	ExtractString(*string, &stringEnd, startIndex, endIndex);
	
//	printf("string ends in: ");
//	BetterPrint(stringEnd);
//	BetterPrint(unityEngineTexture2D);
	
	if (StringsEqual(stringEnd, unityEngineTexture2D) == true)
	{
		ResizeString(string, string->length - unityEngineTexture2D.length);
		GarunteeNullTerminator(string);
//		printf("string trimmed to: ");
//		BetterPrint(*string);
	}
	
	free(stringEnd.chars);
}

// returns the property and its value of a line in a modelData file
KeyValue ParseMeshProperty(String propertyLine)
{
	//			  V (12)
	//..property..:..value.. (22)
	int colonIndex = IndexOfChar(propertyLine, 0, ':');
	
	if (colonIndex == -1)
	{
		//printf("no : found\n");
		return (KeyValue){ 0 };
	}
	
	// extract the property
	String property = { .length = colonIndex, .chars = malloc(sizeof(char) * colonIndex) };
	memcpy(property.chars, propertyLine.chars, sizeof(char) * property.length);
	TrimWhitespace(&property);
	
	// extract the value
	int valueLength = propertyLine.length - colonIndex - 1;
	String value = { .length = valueLength, .chars = malloc(sizeof(char) * valueLength) };
	memcpy(value.chars, &propertyLine.chars[colonIndex + 1], value.length);
	TrimWhitespace(&value);
	TrimUnityEngineTexture2D(&value);
	
	// package into key value pair
	KeyValue propertyValue = (KeyValue){ .key = property, .value = value };
	return propertyValue;
}

//-------------------------------------------
//Mesh Name Model: DoorA1
//Render Type: MeshRenderer
//++++++++++++++++++
//Material #: 0
//   Material Name: futurecsm_fueltank
//	   Shader Name			   : KSP/Bumped Specular
//	   RenderQueue			   : 2000
//	   _MainTex				  : NearFutureSpacecraft/Parts/FuelTank/service-tank/servicetank-25 (UnityEngine.Texture2D)
//	   _MainTexOffset			: (0.0, 0.0)
//	   _BumpMap				  : NearFutureSpacecraft/Parts/FuelTank/service-tank/servicetank-25-n (UnityEngine.Texture2D)
//	   _BumpMapOffset			: (0.0, 0.0)
//	   _Shininess				: 0.3
//	   _Color					: (1.0, 1.0, 1.0, 1.0)
//
//++++++++++++++++++
//-------------------------------------------

// makes a material with all properties 0 or NULL
void NewMaterial(ModelMaterial * material)
{
	*material = (ModelMaterial){ .materialName = (String){ .chars = NULL, .length = 0 }, .shaderName = (String){ .chars = NULL, .length = 0 }, .meshIds = (IntArray){ .ints = NULL, .length = 0 }, .id = UniqueId(), .properties = (KeyValueArray){ .keysValues = NULL, .length = 0 }};
}

// adds a property to a material
void AddProperty(KeyValue property, ModelMaterial * material)
{
	KeyValueArray * properties = &material->properties;
	
//	printf("key: ");
//	BetterPrint(property.key);
//	printf("vaue: ");
//	BetterPrint(property.value);
	
	properties->length += 1;
	properties->keysValues = realloc(properties->keysValues, sizeof(KeyValue) * properties->length);
	properties->keysValues[properties->length - 1] = property;
}

// adds a mesh to the list of meshes that use a material
void AddMesh(int meshId, ModelMaterial * material)
{
	material->meshIds.length += 1;
	material->meshIds.ints = realloc(material->meshIds.ints, sizeof(ModelMesh *) * material->meshIds.length);
	material->meshIds.ints[material->meshIds.length - 1] = meshId;
}

// reads the information about the material that a mesh uses
void MaterialPropertiesFromMesh(ModelMaterial * material, String modelData, int materialStart)
{	
	// first line is minus row
	// second line is mesh name
	// third line is render type
	// fourth line is plus row
	// fifth line is material #
	// sixth line is material name
	// seventh is shader name
	// eighth and onwards are properties and values
	
	int firstNewLine = IndexOfChar(modelData, materialStart + 1, '\n');
	int secondNewLine = IndexOfChar(modelData, firstNewLine + 1, '\n');
	int thirdNewLine = IndexOfChar(modelData, secondNewLine + 1, '\n');
	int fourthNewLine = IndexOfChar(modelData, thirdNewLine + 1, '\n');
	int fifthNewLine = IndexOfChar(modelData, fourthNewLine + 1, '\n');
	int sixthNewLine = IndexOfChar(modelData, fifthNewLine + 1, '\n');
	int seventhNewLine = IndexOfChar(modelData, sixthNewLine + 1, '\n');
	
	String thirdLine = (String){ .chars = NULL, .length = 0 };
	ExtractString(modelData, &thirdLine, secondNewLine + 1, thirdNewLine - 1);
	String fifthLine = (String){ .chars = NULL, .length = 0 };
	ExtractString(modelData, &fifthLine, fourthNewLine + 1, fifthNewLine - 1);
	String sixthLine = (String){ .chars = NULL, .length = 0 };
	ExtractString(modelData, &sixthLine, fifthNewLine + 1, sixthNewLine - 1);
	String seventhLine = (String){ .chars = NULL, .length = 0 };
	ExtractString(modelData, &seventhLine, sixthNewLine + 1, seventhNewLine - 1);
	
//	printf("third line: ");
//	BetterPrint(thirdLine);
//	printf("fifth line: ");
//	BetterPrint(fifthLine);
//	printf("sixth line: ");
//	BetterPrint(sixthLine);
//	printf("seventh line: ");
//	BetterPrint(seventhLine);
	
	AddProperty(ParseMeshProperty(thirdLine), material);
	AddProperty(ParseMeshProperty(fifthLine), material);
	material->materialName = ParseMeshProperty(sixthLine).value;
	material->shaderName = ParseMeshProperty(seventhLine).value;
	
	free(thirdLine.chars);
	free(fifthLine.chars);
	free(sixthLine.chars);
	free(seventhLine.chars);
	
//	BetterPrint(material->materialName);
//	BetterPrint(material->shaderName);
	
	int lineStart;
	int lineEnd = seventhNewLine;
	
	while (true)
	{
		lineStart = lineEnd;
		lineEnd = IndexOfChar(modelData, lineStart + 1, '\n');
		
		String line = (String){ .chars = NULL, .length = 0 };
		ExtractString(modelData, &line, lineStart + 1, lineEnd - 1);
		TrimWhitespace(&line);
		// printf("line: ");
		// BetterPrint(line);
		
		// there is a blank line at the end of each material
		if (line.chars[0] == '\0')
		{
			// reached the end of the material
			//printf("reached material end\n");
			break;
		}
		
		AddProperty(ParseMeshProperty(line), material);
		free(line.chars);
	}
	
	return;
}

void ParseModelData(MuModel * model, String modelData)
{
	// modelData.txt is made up of a series of meshes, which are bordered, on either side, by a row of -'s: -------------------------------------------
	// within that is the mesh name, render type and a material which is bordered, on either side, by a row of +'s: (++++++++++++++++++)
	
	// MuModel contains a list of materials and meshes
	// meshes have a pointer to the material they use
	// materials have an array of pointers to the meshes that use them
	
	int minusRowLength = 43;
	String minusRow = (String){ .length = minusRowLength, .chars = malloc(sizeof(char) * minusRowLength) };
	memcpy(minusRow.chars, "-------------------------------------------", sizeof(char) * minusRow.length);
	
	int plusRowLength = 18;
	String plusRow = (String){ .length = plusRowLength, .chars = malloc(sizeof(char) * plusRowLength) };
	memcpy(plusRow.chars, "++++++++++++++++++", sizeof(char) * plusRow.length);
	
	// find a minus row to start a mesh
	// five lines afterwards will be the name of the material the mesh uses
	// create a new material and parse its proprties from the mesh
	// if that material has already been found, add the mesh name to that material and go to the next mesh
	
	// removing minus length causes the first mesh start index to be at the start of model data, since minus length is added when searching for start index
	int meshEndIndex = -1 * minusRowLength;
	int meshStartIndex = 0;
	
	while(true)
	{
		// find where the mesh starts and ends
		meshStartIndex = IndexOfSubstring(modelData, meshEndIndex + minusRowLength, minusRow);
		meshEndIndex = IndexOfSubstring(modelData, meshStartIndex + minusRowLength, minusRow);
		
		if (meshEndIndex == -1 || meshStartIndex == -1)
		{
			// hit the and of the file
			break;
		}
		
		// a mesh has been found, add it to this model
		model->meshes.length += 1;
		model->meshes.meshes = realloc(model->meshes.meshes, sizeof(ModelMesh) * model->meshes.length);
		ModelMesh * mesh = &model->meshes.meshes[model->meshes.length - 1];
		
		mesh->id = UniqueId();
		// find the name of the mesh
		mesh->name = FindMeshName(modelData, meshStartIndex);
		
		printf("reading mesh: ");
		BetterPrint(mesh->name);
		
		// find the name of the material the mesh uses
		String materialName = FindMaterialName(modelData, meshStartIndex);
		// check if this material has alredy been found
		int materialIndex = HasMaterialWithName(*model, materialName);
		
		// if this material has already been seen, add this mesh to the found material
		if (materialIndex != -1)
		{
			// add this mesh to the list of meshes that the material uses
			ModelMaterial * existingMaterial = &model->materials.materials[materialIndex];
			// add this mesh to the list of meshes that use this material
			AddMesh(mesh->id, existingMaterial);
			// set this material as the material the mesh uses
			mesh->materialId = existingMaterial->id;
			continue;
		}
		
		// if this is a new material
		// add a new material to the model
		model->materials.length += 1;
		model->materials.materials = realloc(model->materials.materials, sizeof(ModelMaterial) * model->materials.length);
		ModelMaterial * newMaterial = &model->materials.materials[model->materials.length - 1];
		NewMaterial(newMaterial);
		
		// add this mesh to the list of meshes that the material uses
		AddMesh(mesh->id, newMaterial);
		// set this material as the material the mesh uses
		mesh->materialId = newMaterial->id;
		
		// get the properties of the new material from the mesh's bit of modelData
		MaterialPropertiesFromMesh(newMaterial, modelData, meshStartIndex);
		
		printf("found material: ");
		BetterPrint(newMaterial->materialName);
	}
	
	printf("found: %d materials\n", model->materials.length);
	
	free(minusRow.chars);
	free(plusRow.chars);
}

// finds the name of a model from its file path
void ModelNameFromModelPath(String modelPath, String * modelName)
{
//	printf("model path: ");
//	BetterPrint(modelPath);
	
	// .......V.....  	7
	// cheese/name\0  	12
	// name\0			5
	
	// the name starts after the last /
	int nameIndex = LastIndexOfChar(modelPath, modelPath.length - 1, '/') + 1;
	int nameLength = modelPath.length - nameIndex;
	// copy the stuff after the slash to the model name
	ResizeString(modelName, nameLength);
	memcpy(modelName->chars, &modelPath.chars[nameIndex], sizeof(char) * nameLength);
	// add .mu to the end of the name
	ResizeString(modelName, modelName->length + 3);
	modelName->chars[modelName->length - 4] = '.';
	modelName->chars[modelName->length - 3] = 'm';
	modelName->chars[modelName->length - 2] = 'u';
	modelName->chars[modelName->length - 1] = '\0';
	
//	printf("model name: ");
//	BetterPrint(*modelName);
}

// parse an array of model data files
void ParseModelDatas(ModelArray * models, StringArray modelDatas, StringArray modelPaths)
{
	// clear out the models
	*models = (ModelArray){ .models = NULL, .length = 0 };
	
	for (int index = 0; index < modelDatas.length; index ++)
	{
		// what was read from ModelData.txt
		String modelData = modelDatas.strings[index];
		
		// create a new model to parse modelData into
		models->length += 1;
		models->models = realloc(models->models, sizeof(Model) * models->length);
		MuModel * model = &models->models[models->length - 1];
		*model = (MuModel){ .materials = (MaterialArray){ .materials = NULL, .length = 0 }, .meshes = (MeshArray){ .meshes = NULL, .length = 0 }, .name = (String){ .chars = NULL, .length = 0 }};
		
		// if failed to read modelData.txt
		if (modelData.length == 0)
		{
			// set the model name to indicate failure to load ModelData.txt
			model->name = (String){ .chars = "ERROR: failed to read ModelData.txt", .length = 36 };
			continue;
		}
		
		printf("Model data file contents:\n%s\n", modelData.chars);
		// find the name of the model from the end of its file path
		ModelNameFromModelPath(modelPaths.strings[index], &model->name);
		// find the materials that this model uses
		ParseModelData(model, modelData);
	}
	
	printf("%d models\n", models->length);
}

// returns every child module that has a specific name
void SearchForPropertyKey(Module * module, String key, KeyValueArray * foundProperties)
{
	// check if any of the properties have the key
	int keyIndex = -1;
	while (true)
	{
		keyIndex = IndexOfKey(module->properties, keyIndex + 1, key);
		
		// if there are no more properties with this key
		if (keyIndex == -1)
		{
			return;
		}
		
		// if a property has been found with the key, add it to the list of found properties
		foundProperties->length += 1;
		foundProperties->keysValues = realloc(foundProperties->keysValues, sizeof(KeyValue) * foundProperties->length);
		foundProperties->keysValues[foundProperties->length - 1] = module->properties.keysValues[keyIndex];
	}
	
	// check child modules
	for (int index = 0; index < module->children.length; index ++)
	{
		Module * child = &module->children.modules[index];
		SearchForPropertyKey(child, key, foundProperties);
	}
}

// returns every child module that is of a specific type
void SearchForModuleType(Module * module, String type, ModuleArray * foundModules)
{
	// if this module has the correct type add it to the list of found modules
	if (StringsEqual(module->type, type) == true)
	{
		foundModules->length += 1;
		foundModules->modules = realloc(foundModules->modules, sizeof(Module) * foundModules->length);
		foundModules->modules[foundModules->length - 1] = *module;
	}
	
	// search through child modules
	for (int index = 0; index < module->children.length; index ++)
	{
		Module * child = &module->children.modules[index];
		SearchForModuleType(child, type, foundModules);
	}
}

// returns the file path after the "model =" in each of the model modules
// and adds the file path of the part config to the start of mesh properties
// output is in the format: ModName/ect.../ModelName
void GetModelPaths(ModuleArray modelModules, KeyValueArray meshProperties, PartInformation part, StringArray * output)
{
	output->strings = realloc(output->strings, 0);
	output->length = 0;
	
	// find the path of each of the model modules
	for (int index = 0; index < modelModules.length; index ++)
	{
		Module modelModule = modelModules.modules[index];
		String modelPath = (String){ .chars = NULL, .length = 0 };
		ValueOfKey(modelModule.properties, (String){ .chars = "model", .length = 6 }, &modelPath);
		
		if (modelPath.length == 0)
		{
			printf("failed to find key: \"module\"\n");
			continue;
		}
		
		output->length += 1;
		output->strings = realloc(output->strings, sizeof(String) * output->length);
		output->strings[output->length - 1] = modelPath;
	}
	
	// find the part of each of the mesh properties
	String partFilePath = part.configFilePath;
	for (int index = 0; index < meshProperties.length; index ++)
	{
		output->length += 1;
		output->strings = realloc(output->strings, sizeof(String) * output->length);
		String * modelPath = &output->strings[output->length - 1];
		*modelPath = (String){ .chars = NULL, .length = 0 };
		
		KeyValue meshProperty = meshProperties.keysValues[index];
		String modelName = meshProperty.value;
		
		// get rid of everything after the last / of the parts file path and add on the name of the model
		// copy the parts file path so we can edit it
		ResizeString(modelPath, partFilePath.length);
		memcpy(modelPath->chars, partFilePath.chars, sizeof(char) * partFilePath.length);
		
		int lastSlashIndex = LastIndexOfChar(partFilePath, partFilePath.length - 1, '/');
		int newLength = lastSlashIndex + modelName.length;
		ResizeString(modelPath, newLength);
		memcpy(&modelPath->chars[lastSlashIndex + 1], modelName.chars, sizeof(char) * modelName.length);
		
		// get rid of the .mu extenstion if it exists
		int muIndex = IndexOfSubstring(*modelPath, 0, (String){ .chars = ".mu", .length = 3 });
		if (muIndex != -1)
		{
			ResizeString(modelPath, modelPath->length - 3);
			GarunteeNullTerminator(modelPath);
		}
		
		// get rid of the leading GameData/, if it exists
		int gameDataIndex = IndexOfSubstring(*modelPath, 0, (String){ .chars = "GameData/", .length = 9 });
		if (gameDataIndex != -1)
		{
			// move to where gamedata ends
			gameDataIndex += 9;
			int charsAfterGamedata = modelPath->length - gameDataIndex;
			memmove(modelPath->chars, &modelPath->chars[gameDataIndex], charsAfterGamedata);
			ResizeString(modelPath, modelPath->length - gameDataIndex);
			GarunteeNullTerminator(modelPath);
		}
	}
}

// load the modelData.txt files associated with the part
// there will be one for each model the part uses
void LoadModelData(PartInformation * part)
{
	// find all the "MODEL" modules a part, which will contain a model
	ModuleArray modelModules = (ModuleArray){ .modules = NULL, .length = 0 };
	SearchForModuleType(&part->modules, (String){ .chars = "MODEL", .length = 6 }, &modelModules);
	printf("\nfound: %d model modules\n", modelModules.length);
	
	// models can also be defined with: mesh = model.mu
	// this is probably only valid in the PART module, but we search the whole tree, just in case
	KeyValueArray meshProperties = (KeyValueArray){ .keysValues = NULL, .length = 0 };
	SearchForPropertyKey(&part->modules, (String){ .chars = "mesh", .length = 5 }, &meshProperties);
	printf("found: %d mesh properties\n", meshProperties.length);
	// in this case it is assumed that the model.mu is in the same folder as the config file
	
	// find the path to each model
	StringArray modelPaths = (StringArray){ .strings = NULL, .length = 0 };
	GetModelPaths(modelModules, meshProperties, *part, &modelPaths);
	
	// covert the model path to the path to their modelData.txt
	StringArray modelDataPaths = (StringArray){ .strings = NULL, .length = 0 };
	ModelPathsToModelDataPaths(modelPaths, &modelDataPaths);
	
	// read each of the model data files
	StringArray modelDataFilesContents = (StringArray){ .strings = NULL, .length = 0 };
	ReadFiles(modelDataPaths, &modelDataFilesContents);
	FreeStringArray(&modelDataPaths);
	
	// parse each of the model data files into an array of materials
	ParseModelDatas(&part->models, modelDataFilesContents, modelPaths);
	
	FreeStringArray(&modelPaths);
	FreeStringArray(&modelDataFilesContents);
}

// finds the mesh in a part that has a specific id
ModelMesh * GetMeshWithId(PartInformation part, int id)
{
	for (int modelIndex = 0; modelIndex < part.models.length; modelIndex ++)
	{
		MuModel * model = &part.models.models[modelIndex];
		
		for (int meshIndex = 0; meshIndex < model->meshes.length; meshIndex ++)
		{
			ModelMesh * mesh = &model->meshes.meshes[meshIndex];
			
			if (mesh->id == id)
			{
				return mesh;
			}
		}
	}
	
	printf("WARNING: failed to find mesh with id: %d\n", id);
	return NULL;
}

//█▀▄ █ █▀█ █▀▀ █▀▀ ▀█▀ █▀█ █▀█ █▄█   █▄░█ ▄▀█ █░█ █ █▀▀ ▄▀█ ▀█▀ █ █▀█ █▄░█
//█▄▀ █ █▀▄ ██▄ █▄▄ ░█░ █▄█ █▀▄ ░█░   █░▀█ █▀█ ▀▄▀ █ █▄█ █▀█ ░█░ █ █▄█ █░▀█

// checks if a string contains a .
bool ContainsDot(String fileName)
{
	int dotIndex = IndexOfChar(fileName, 0, '.');
	
	// check if a dot was found
	if (dotIndex == -1)
	{
		//printf("no dot found\n");
		return false;
	}
	//printf("dot found\n");
	return true;
}

// checks is a string is .
bool IsDot(String fileName)
{
	// check for "."
	if (fileName.length < 1)
	{
		return false;
	}
	else if (fileName.length == 1 && fileName.chars[0] == '.')
	{
		return true;
	}
	// check for ".\0"
	if (fileName.length < 2)
	{
		return false;
	}
	else if (fileName.length == 2 && fileName.chars[0] == '.' && fileName.chars[0] == '\0')
	{
		return true;
	}
	return false;
}

// checkis if a string is ..
bool IsDotDot(String fileName)
{
	// check for ".."
	if (fileName.length < 2)
	{
		return false;
	}
	else if (fileName.length == 2 && fileName.chars[0] == '.' && fileName.chars[1] == '.')
	{
		return true;
	}
	// check for "..\0"
	if (fileName.length < 3)
	{
		return false;
	}
	else if (fileName.length == 3 && fileName.chars[0] == '.' && fileName.chars[1] == '.' && fileName.chars[2] == '\0')
	{
		return true;
	}
	return false;
}

// check if a string is .. or .
bool IsNavigationDots(String fileName)
{
	if (IsDot(fileName) == false && IsDotDot(fileName) == false)
	{
		//printf("not . or ..\n");
		return false;
	}
	
	//printf("is . or ..\n");
	return true;
}

// removes the last directory from the directory path
void RemoveLastDirectory(String * directoryPath)
{
	//find the index of the second to last / and remove everything after that
	// abc/xyz/123/\0
	//printf("clicked on a this: ..\n");
	
	int secondToLastSlashIndex = LastIndexOfChar(*directoryPath, directoryPath->length - 3, '/');
	
	//printf("stlsi: %d\n", secondToLastSlashIndex);
	
	ResizeString(directoryPath, secondToLastSlashIndex + 2);
	directoryPath->chars[directoryPath->length - 1] = '\0';
}

// finds the type of file extension
bool FileExtensionMatches(String fileName, String targetExtension)
{
	// xyz.abc\0
	// .abc
	// working from the end, check if each char matches, ignoring \0 at the start of the filename
	
	int targetIndex = targetExtension.length - 1;
	int fileIndex = fileName.length;
	
	if (HasNullTerminator(fileName) == true)
	{
		fileIndex -= 1;
	}
	
	int targetLength = targetExtension.length;
	
	// ignore trailing \0
	while (fileName.chars[fileIndex] == '\0')
	{
		fileIndex --;
	}
	
	while (targetExtension.chars[targetIndex] == '\0')
	{
		targetIndex --;
		targetLength --;
	}
	
	for (int index = 0; index < targetLength; index ++)
	{
	
		//printf("comparing: %c and %c\n", fileName.chars[fileIndex], targetExtension.chars[targetIndex]);
		
		if (fileName.chars[fileIndex] != targetExtension.chars[targetIndex])
		{
			//printf("%c != %c\n", fileName.chars[fileIndex], targetExtension.chars[targetIndex]);
			return false;
		}
		
		fileIndex --;
		targetIndex --;
	}
	
	return true;
}

// add a new directory to the end of the current file path
String ConcatenateFileNames(String currentDirectory, String newDirectory)
{
	GarunteeNullTerminator(&currentDirectory);
	GarunteeNullTerminator(&newDirectory);
	
//	printf("combining: ");
//	BetterPrint(currentDirectory);
//	printf("and: ");
//	BetterPrint(newDirectory);
	
//	printf("current directory: ");
//	BetterPrint(currentDirectory);
//	
//	printf("new directory: ");
//	BetterPrint(newDirectory);
	
	// newDirectory should include the null terminator
	// currentDirectory should include null terminator
	
	// abc/\0 			(length 5)
	// xyz\0			(length 4)
	// -> abc/xyz/\0	(length 9)
	
	// find the index where the name should be added
	int nameInsertionIndex = currentDirectory.length - 1;
	// new length of the directory name
	int newDirectoryNameLength = currentDirectory.length + newDirectory.length;
	
	ResizeString(&currentDirectory, newDirectoryNameLength);
	
	// add the name, / and \0
	memcpy(&currentDirectory.chars[nameInsertionIndex], newDirectory.chars, sizeof(char) * newDirectory.length);
	// abc/xyz
	currentDirectory.chars[currentDirectory.length - 1] = '\0';
	currentDirectory.chars[currentDirectory.length - 2] = '/';
	// abc/xyz/\0
	
//	printf("total file path: ");
//	BetterPrint(currentDirectory);
	
	return currentDirectory;
}

// navigate the file depending on its type
// folders are opened as the current directory
// .cfg files are sent to be loaded into the part
void NavigateDirectory(String * selectedDirectory, String clickedDirectory, PartInformation * part)
{
	// check the type, if it is a folder add its name and a / to the selectedDirectory
	
	// if it is . dont do anything
	if (IsDot(clickedDirectory) == true)
	{
		return;
	}
	
	// if it is .. remove the last part of the selectedDirectory, unless the selectedDirectory ends in ../ if does add the new ../ instead
	if (IsDotDot(clickedDirectory) == true && FileExtensionMatches(*selectedDirectory, (String){ .length = 4, .chars = "../"}) == false)
	{
		RemoveLastDirectory(selectedDirectory);
		return;
	}
	
	// check if it is a file, by checking if it contains a dot but is not . or ..
	if (ContainsDot(clickedDirectory) == true && IsNavigationDots(clickedDirectory) == false)
	{
		//check if it is a .cfg file that has been clicked on
		if (FileExtensionMatches(clickedDirectory, (String){ .length = 5, .chars = ".cfg"}) == true)
		{
			printf("clicked on a .cfg file\n");
			
			// combine the directory path and the filename
			String filePath = (String){ .chars = NULL, .length = 0 };
			ConcatenateStrings(*selectedDirectory, clickedDirectory, &filePath);
			
			// try to load the config file as a string
			if (LoadConfigFile(part, filePath) == true)
			{
				// if that succeded, try and load ModelData.txt
				LoadModelData(part);
			}
			
			free(filePath.chars);
			return;
		}
		
		printf("clicked on a not .cfg file\n");
		return;
	}
	
	// the new directory is a folder, add it to the directory tree
	*selectedDirectory = ConcatenateFileNames(*selectedDirectory, clickedDirectory);
	
	printf("current directory: ");
	BetterPrint(*selectedDirectory);
	
	return;
}

//▀█▀ █▀▀ ▀▄▀ ▀█▀ █░█ █▀█ █▀▀ █▀
//░█░ ██▄ █░█ ░█░ █▄█ █▀▄ ██▄ ▄█

// loads all of the images in the textures directory into the texture atlas
void LoadTextureAtlas()
{
	String texturesLocation = (String){ .chars = "Textures/", .length = 10 };
	DIR * texturesDirectory = opendir(texturesLocation.chars);
	
	if (texturesDirectory == NULL)
	{
		int err = errno;
		printf("ERROR: Could not open: %s because: %s\n", texturesLocation.chars, strerror(err));
		return;
	}
	
	// create the texture atlas
	NamedTextureArray * textures = malloc(sizeof(NamedTextureArray));
	textures->length = 1;
	textures->textures = malloc(sizeof(NamedTexture));
	// set a pink square as the first textures, this will be used if a texture cannot be found in the atlas
	textures->textures[0] = (NamedTexture){ .name = "_NOT_FOUND", .texture = LoadTextureFromImage(GenImageColor(10, 10, MAGENTA)) };
	
	textureAtlas = textures;
	
	// read all of the entries in the directory and load all .png files as textures
	struct dirent * entry = readdir(texturesDirectory);
	
	while (entry != NULL)
	{
		int entryNameLength = strlen(entry->d_name);
		//printf("entry name length: %d\n", entryNameLength);
		//printf("entry name: %s\n", entry->d_name);
		
		// check if the entry name ends in .png
		if (FileExtensionMatches((String){ .chars = entry->d_name, .length = entryNameLength }, (String){ .chars = ".png", .length = 5 }) == true)
		{
			// build the path to the texture from the game executable
			// start with Textures/
			String texturePath = (String){ .chars = NULL, .length = 0 };
			ResizeString(&texturePath, texturesLocation.length);
			memcpy(texturePath.chars, texturesLocation.chars, sizeof(char) * texturesLocation.length);
			// add the name of the .png file
			ResizeString(&texturePath, texturePath.length + entryNameLength);
			memcpy(&texturePath.chars[texturesLocation.length - 1], entry->d_name, sizeof(char) * entryNameLength);
			
			// load the .png as a texture
			NamedTexture newTexture = (NamedTexture){ .texture = LoadTexture(texturePath.chars), .name = (String){ .chars = NULL, .length = 0 } };
			// name the texture after its filename
			ResizeString(&newTexture.name, entryNameLength + nullTerminator);
			memcpy(newTexture.name.chars, entry->d_name, sizeof(char) * entryNameLength);
			
			printf("sprite loaded: ");
			BetterPrint(newTexture.name);
			
			// add the new texture to the array
			textures->length += 1;
			textures->textures = realloc(textures->textures, textures->length * sizeof(NamedTexture));
			textures->textures[textures->length - 1] = newTexture;
			
			free(texturePath.chars);
		}
		// read the next entry in the directory
		entry = readdir(texturesDirectory);
	}
	
	closedir(texturesDirectory);
}

// find a texture with a specific name
Texture * GetTextureWithName(String name)
{
	for (int index = 0; index < textureAtlas->length; index ++)
	{
		if (StringsEqual(textureAtlas->textures[index].name, name) == true)
		{
//			printf("found texture: ");
//			BetterPrint(name);
			return &textureAtlas->textures[index].texture;
		}
	}
	
//	printf("could not find texture: ");
//	BetterPrint(name);
	
	// return the first texture, which should be a pink square
	Texture * notFound = &textureAtlas->textures[0].texture;
	
	return notFound;
}

void LoadEmbossedTextures()
{
	// create embossed textures holder
	EmbossedTextures * textures = malloc(sizeof(EmbossedTextures));
	embossedTextures = textures;
	
	// find the embossed textures in the texture atlas
	textures->topLeft = GetTextureWithName((String){ .chars = "EmbossedTopLeft.png", .length = 20 });
	textures->topLeftInverse = GetTextureWithName((String){ .chars = "EmbossedTopLeftInverse.png", .length = 27 });
	textures->top = GetTextureWithName((String){ .chars = "EmbossedTop.png", .length = 16 });
	textures->topRight = GetTextureWithName((String){ .chars = "EmbossedTopRight.png", .length = 21 });
	textures->topRightInverse = GetTextureWithName((String){ .chars = "EmbossedTopRightInverse.png", .length = 28 });
	textures->left = GetTextureWithName((String){ .chars = "EmbossedLeft.png", .length = 17 });
	textures->right = GetTextureWithName((String){ .chars = "EmbossedRight.png", .length = 18 });
	textures->bottomLeft = GetTextureWithName((String){ .chars = "EmbossedBottomLeft.png", .length = 23 });
	textures->bottomLeftInverse = GetTextureWithName((String){ .chars = "EmbossedBottomLeftInverse.png", .length = 30 });
	textures->bottom = GetTextureWithName((String){ .chars = "EmbossedBottom.png", .length = 19 });
	textures->bottomRight = GetTextureWithName((String){ .chars = "EmbossedBottomRight.png", .length = 24 });
	textures->bottomRightInverse = GetTextureWithName((String){ .chars = "EmbossedBottomRightInverse.png", .length = 31 });
}

//TODO:
//█▀█ █▀▀ █▀▀ █▀█ █░░ █▀█ █░█ █▀█ █ █▄░█ █▀▀
//█▀▄ ██▄ █▄▄ █▄█ █▄▄ █▄█ █▄█ █▀▄ █ █░▀█ █▄█

// ModelMaterial => textureSwitch with 2x textureSets
// textureSets => recolour configs

// list of ksp's built in shaders: https://forum.kerbalspaceprogram.com/topic/83971-list-of-loaded-shaders/#findComment-4037180

// relevant shader types that show up on parts:
// KSP/Diffuse
// KSP/Specular
// KSP/Specular (Transparent)
// KSP/Bumped
// KSP/Bumped Specular
// KSP/Bumped Specular (Mapped)
// KSP/Bumped Specular (Stencil)
// KSP/Bumped Specular (Transparent)
// KSP/Emissive/Bumped Specular
// KSP/Emissive/Bumped Specular (Mapped)
// KSP/Emissive/Diffuse
// KSP/Emissive/Specular

// irelevant shader types that show up on parts:
// KSP/Alpha/ect...
// KSP/Particles/ect...

// everything else shouldnt show up on parts, or i don't know about it

typedef enum ShaderType
{
	DIFFUSE,
	SPECULAR,
	SPECULAR_TRANSPARENT,
	BUMPED,
	BUMPED_SPECULAR,
	BUMPED_SPECULAR_MAPPED,
	BUMPED_SPECULAR_STENCIL,	// idk what this one is, never seen it in the wild
	BUMPED_SPECULAR_TRANSPARENT,
	EMISSIVE_BUMPED_SPECULAR,
	EMISSIVE_BUMPED_SPECULAR_MAPPED,
	EMISSIVE_DIFFUSE,
	EMISSIVE_SPECULAR,
	IRRELEVANT,			// the exact type of irrelevant shaders does not need to be known
	UNKNOWN,
	// VV tu shaders VV
	TU_METALLIC,
	TU_SPECULAR,
	TU_LEGACY,
	TU_TRANSPARENT_METALLIC,
	TU_TRANSPARENT_SPECULAR,
	TU_TRANSPARENT_LEGACY,
	TU_DIFFUSE,				// not a real shader, indicates that the original material used ksp/diffuse and therfore has no specular map in its _maintex, and needs a new one
	// VV resurfaced shaders VV
	RESURFACED,			// TODO: handling for the different types of resurfaced shaders
	// VV shaddy shaders VV
	SHADDY							// TODO: handling for the different types of shaddy shaders
}
ShaderType;

// relevant
String diffuse = (String){ .chars = "KSP/Diffuse", .length = 12 };
String specular = (String){ .chars = "KSP/Specular", .length = 13 };
String specularTransparent = (String){ .chars = "KSP/Specular (Transparent)", .length = 27 };
String bumped = (String){ .chars = "KSP/Bumped", .length = 11 };
String bumpedSpecular = (String){ .chars = "KSP/Bumped Specular", .length = 20 };
String bumpedSpecularMapped = (String){ .chars = "KSP/Bumped Specular (Mapped)", .length = 29 };
String bumpedSpecularStencil = (String){ .chars = "KSP/Bumped Specular (Stencil)", .length = 30 };
String bumpedSpecularTransparent = (String){ .chars = "KSP/Bumped Specular (Transparent)", .length = 34 };
String emissiveBumpedSpecular = (String){ .chars = "KSP/Emissive/Bumped Specular", .length = 29 };
String emissiveBumpedSpecularMapped = (String){ .chars = "KSP/Emissive/Bumped Specular (Mapped)", .length = 38 };
String emissiveDiffuse = (String){ .chars = "KSP/Emissive/Diffuse", .length = 21 };
String emissiveSpecular = (String){ .chars = "KSP/Emissive/Specular", .length = 22 };

String tuMetallic = (String){ .chars = "TU/Metallic", .length = 12 };
String tuSpecular = (String){ .chars = "TU/Specular", .length = 12 };
String tuLegacy = (String){ .chars = "TU/Legacy", .length = 10 };
String tuTransparentMetallic = (String){ .chars = "TU/Transparent/Metallic", .length = 24 };
String tuTransparentSpecular = (String){ .chars = "TU/Transparent/Specular", .length = 24 };
String tuTransparentLegacy = (String){ .chars = "TU/Transparent/Legacy", .length = 22 };

String resurfaced = (String){ .chars = "Resurfaced", .length = 10 };
String shaddy = (String){ .chars = "Shaddy", .length = 7 };

// irrelevant
// lengths are one less than normal because we dont want to include the null terminator
String alpha = (String){ .chars = "KSP/Alpha", .length = 9 };
String particles = (String){ .chars = "KSP/Particles", .length = 13 };

// texture sets, used for reconstructing the default material of the part
// and for recolouring the part
typedef struct TextureSet
{
	String name;
	String title;			// this is what the texture set is called in game
	// VV material properties VV
	bool recolourable;
	String shaderName;
	ShaderType shaderType;
	StringArray meshNames;
	KeyValueArray properties;
}
TextureSet;

typedef struct TextureSetArray
{
	TextureSet * textureSets;
	int length;
}
TextureSetArray;

typedef struct TextureSwitch
{
	String name;					// in game switcher name
	TextureSet * currentTextureSet;	// the texture set that is selected to begin with
	TextureSetArray textureSets;	// available texture sets
}
TextureSwitch;

// all the switches of a single part
typedef struct TextureSwitchArray
{
	TextureSwitch * textureSwitches;
	int length;
}
TextureSwitchArray;

// find the shader used by the material that a mesh uses
ShaderType GetShaderType(ModelMaterial material)
{
	// find the name of the shader
	String shaderName = material.shaderName;
	
	printf("material: ");
	BetterPrint(material.materialName);
	printf("uses shader: ");
	BetterPrint(shaderName);
	
	// irrelevant
	if (StringStartsWith(shaderName, alpha) == true || StringStartsWith(shaderName, particles) == true)
	{
		printf("material: %s uses irrelevant shader type: %s\n", material.materialName.chars, shaderName.chars);
		return IRRELEVANT;
	}
	// not suppoted
	else if (StringStartsWith(shaderName, resurfaced) == true)
	{
		printf("WARNING: material: %s uses unsupported shader type: %s\n", material.materialName.chars, shaderName.chars);
		return RESURFACED;
	}
	else if (StringStartsWith(shaderName, shaddy) == true)
	{
		printf("WARNING: material: %s uses unsupported shader type: %s\n", material.materialName.chars, shaderName.chars);
		return SHADDY;
	}
	
	// theres gotta be a better way to do this
	else if (StringsEqual(shaderName, diffuse) == true)
	{
		return DIFFUSE;
	}
	else if (StringsEqual(shaderName, specular) == true)
	{
		return SPECULAR;
	}
	else if (StringsEqual(shaderName, specularTransparent) == true)
	{
		return SPECULAR_TRANSPARENT;
	}
	else if (StringsEqual(shaderName, bumped) == true)
	{
		return BUMPED;
	}
	else if (StringsEqual(shaderName, bumpedSpecular) == true)
	{
		return BUMPED_SPECULAR;
	}
	else if (StringsEqual(shaderName, bumpedSpecularMapped) == true)
	{
		return BUMPED_SPECULAR_MAPPED;
	}
	else if (StringsEqual(shaderName, bumpedSpecularStencil) == true)
	{
		return BUMPED_SPECULAR_STENCIL;
	}
	else if (StringsEqual(shaderName, bumpedSpecularTransparent) == true)
	{
		return BUMPED_SPECULAR_TRANSPARENT;
	}
	else if (StringsEqual(shaderName, emissiveBumpedSpecular) == true)
	{
		return EMISSIVE_BUMPED_SPECULAR;
	}
	else if (StringsEqual(shaderName, emissiveBumpedSpecularMapped) == true)
	{
		return EMISSIVE_BUMPED_SPECULAR_MAPPED;
	}
	else if (StringsEqual(shaderName, emissiveDiffuse) == true)
	{
		return EMISSIVE_DIFFUSE;
	}
	else if (StringsEqual(shaderName, emissiveSpecular) == true)
	{
		return EMISSIVE_SPECULAR;
	}
	else if (StringsEqual(shaderName, tuMetallic) == true)
	{
		return TU_METALLIC;
	}
	else if (StringsEqual(shaderName, tuSpecular) == true)
	{
		return TU_SPECULAR;
	}
	else if (StringsEqual(shaderName, tuLegacy) == true)
	{
		return TU_LEGACY;
	}
	else if (StringsEqual(shaderName, tuTransparentMetallic) == true)
	{
		return TU_TRANSPARENT_METALLIC;
	}
	else if (StringsEqual(shaderName, tuTransparentSpecular) == true)
	{
		return TU_TRANSPARENT_SPECULAR;
	}
	else if (StringsEqual(shaderName, tuTransparentLegacy) == true)
	{
		return TU_TRANSPARENT_LEGACY;
	}
	
	printf("WARNING: material: %s uses unknown shader type: %s automatic recolor creation not possible\n", material.materialName.chars, shaderName.chars);
	return UNKNOWN;
}

// selects an appropritare recolour shader depending on the the original shader type
ShaderType SelectRecolourShaderType(ShaderType original)
{
	if (original == DIFFUSE || original == EMISSIVE_DIFFUSE || original == BUMPED)
	{
		return TU_DIFFUSE;
	}
	else if (original == SPECULAR || original == BUMPED_SPECULAR || original == BUMPED_SPECULAR_MAPPED || original == EMISSIVE_BUMPED_SPECULAR || original == EMISSIVE_BUMPED_SPECULAR_MAPPED || original == EMISSIVE_SPECULAR || original == BUMPED_SPECULAR_STENCIL)
	{
		return TU_SPECULAR;
	}
	else if (original == SPECULAR_TRANSPARENT || original == BUMPED_SPECULAR_TRANSPARENT)
	{
		return TU_TRANSPARENT_SPECULAR;
	}
	else if (original == TU_LEGACY || original == TU_METALLIC || original == TU_SPECULAR || original == TU_TRANSPARENT_LEGACY || original == TU_TRANSPARENT_METALLIC || original == TU_TRANSPARENT_SPECULAR)
	{
		return original;
	}
	// i'm not actually sure which resurfaced / shaddy shader is used
	// so for now i'm just assuming it can use the tu specular for recoulouring
	else if (original == RESURFACED || original == SHADDY)
	{
		return TU_SPECULAR;
	}
	else if (original == UNKNOWN)
	{
		printf("WARNING: trying to recolour unknown shader type: %d unknown.\n", original);
		return TU_SPECULAR;
	}
	else if (original == IRRELEVANT)
	{
		printf("WARNING: trying to recolour irrelevant shader: %d.\n", original);
		return TU_SPECULAR;
	}
	else
	{
		// this should never run
		printf("ERROR: this should never run, call the dev!\n");
		return TU_SPECULAR;
	}
}

// returns the name associated with the shader types used for recolouring
void RecolourShaderTypeName(ShaderType type, String * name)
{
	switch (type)
	{
		case TU_DIFFUSE:
			*name = tuSpecular;
			break;
			
		case TU_LEGACY:
			*name = tuLegacy;
			break;
			
		case TU_SPECULAR:
			*name = tuSpecular;
			break;
			
		case TU_METALLIC:
			*name = tuMetallic;
			break;
			
		case TU_TRANSPARENT_LEGACY:
			*name = tuTransparentLegacy;
			break;
			
		case TU_TRANSPARENT_SPECULAR:
			*name = tuTransparentSpecular;
			break;
			
		case TU_TRANSPARENT_METALLIC:
			*name = tuTransparentMetallic;
			break;
			
		default:
			printf("WARNING: type not used for recolouring.\n");
			*name = tuSpecular;
	}
}

// checks if two materials are equal
bool MaterialsEqual(ModelMaterial a, ModelMaterial b)
{
	if (StringsEqual(a.materialName, b.materialName) == false)
	{
		return false;
	}
	if (StringsEqual(a.shaderName, b.shaderName) == false)
	{
		return false;
	}
	
	KeyValueArray aProperties = a.properties;
	KeyValueArray bProperties = b.properties;
	
	if (aProperties.length != bProperties.length)
	{
		return false;
	}
	
	for (int index = 0; index < aProperties.length; index ++)
	{
		if (KeyValuesEqual(aProperties.keysValues[index], bProperties.keysValues[index]) == false)
		{
			return false;
		}
	}
	
	return true;
}

// returns the index of a material, or -1 if it is not found
int IndexOfMaterial(MaterialArray materials, ModelMaterial material)
{
	for (int index = 0; index < materials.length; index ++)
	{
		if (MaterialsEqual(materials.materials[index], material) == true)
		{
			return index;
		}
	}
	
	return -1;
}

// the material will have some textures
String mainTex = (String){ .chars = "_MainTex", .length = 9 };
String bumpMap = (String){ .chars = "_BumpMap", .length = 9 };
String emissive = (String){ .chars = "_Emissive", .length = 10 };
String metalMap = (String){ .chars = "_MetalMap", .length = 10 };
String specMap = (String){ .chars = "_SpecMap", .length = 9 };

// in a texture set these are written:
// texture = _MainTex,Tantares/parts/core_vostok/a1

bool PropertyIsTexture(KeyValue property)
{
	if (StringsEqual(property.key, mainTex) == true || StringsEqual(property.key, bumpMap) == true || StringsEqual(property.key, emissive) == true || StringsEqual(property.key, metalMap) == true || StringsEqual(property.key, specMap) == true)
	{
		return true;
	}
	
	return false;
}

// the material will have some single floats with values like: 0.4
String shininess = (String){ .chars = "_Shininess", .length = 11 };
String metalness = (String){ .chars = "_Metalness", .length = 11 };
String smoothness = (String){ .chars = "_Smoothness", .length = 12 };
String metalAlbedoMultiplier = (String){ .chars = "_MetalAlbedoMultiplier", .length = 23 };

// written as:
//	PROPERTY
//	{
//		name = _Shininess
//		float = 0.4
//	}

bool PropertyIsFloat(KeyValue property)
{
	if (StringsEqual(property.key, shininess) == true || StringsEqual(property.key, metalness) == true || StringsEqual(property.key, smoothness) == true || StringsEqual(property.key, metalAlbedoMultiplier) == true)
	{
		return true;
	}
	
	return false;
}

// four float colours with values like: (1.0, 1.0, 1.0, 1.0)
String color = (String){ .chars = "_Color", .length = 7 };
String emissiveColor = (String){ .chars = "_EmissiveColor", .length = 15 };

// written as:
//	PROPERTY
//	{
//		name = _Color
//		color = 1.0, 1.0, 1.0, 1.0
//	}

bool PropertyIsColor(KeyValue property)
{
	if (StringsEqual(property.key, color) == true || StringsEqual(property.key, emissiveColor) == true)
	{
		return true;
	}
	
	return false;
}

// two float offsets with values like: (0, 0)
String mainTexOffset = (String){ .chars = "_MainTexOffset", .length = 15 };
String bumpMapOffset = (String){ .chars = "_BumpMapOffset", .length = 15 };
String emissiveOffset = (String){ .chars = "_EmissiveOffset", .length = 16 };

// written as:
// TODO: find how these are written, i saw it in the TU code somewhere once, but it cant find it again :(

bool PropertyIsOffset(KeyValue property)
{
	if (StringsEqual(property.key, mainTexOffset) == true || StringsEqual(property.key, bumpMapOffset) == true || StringsEqual(property.key, emissiveOffset) == true)
	{
		return true;
	}
	
	return false;
}

// TU shaders have three float vectors
String diffuseNorm = (String){ .chars = "_DiffuseNorm", .length = 13 };
String metalNorm = (String){ .chars = "_MetalNorm", .length = 11 };
String specularNorm = (String){ .chars = "_SpecularNorm", .length = 14 };
String smoothnessNorm = (String){ .chars = "_SmoothnessNorm", .length = 16 };

// written as:
// TODO: find out how these are written

bool PropertyIsVector(KeyValue property)
{
	if (StringsEqual(property.key, diffuseNorm) == true || StringsEqual(property.key, metalNorm) == true || StringsEqual(property.key, specularNorm) == true || StringsEqual(property.key, smoothnessNorm) == true)
	{
		return true;
	}
	
	return false;
}

// there are also some propeties that are probably irrelevant
String renderType = (String){ .chars = "Render Type", .length = 12 };
String materialNumber = (String){ .chars = "Material #", .length = 11 };
String renderQueue = (String){ .chars = "RenderQueue", .length = 12 };

bool PropertyIsIrrelevant(KeyValue property)
{
	if (StringsEqual(property.key, renderType) == true || StringsEqual(property.key, materialNumber) == true || StringsEqual(property.key, renderQueue) == true)
	{
		return true;
	}
	
	return false;
}

// check if this property is known about to the program
bool PropertyIsKnown(KeyValue property)
{
	if (PropertyIsColor(property) == true || PropertyIsFloat(property) == true || PropertyIsIrrelevant(property) == true || PropertyIsOffset(property) == true || PropertyIsVector(property) == true || PropertyIsTexture(property) == true)
	{
		return true;
	}
	
	return false;
}

// default values
String renderTypeDefault = (String){ .chars = "MeshRenderer", .length = 13 };
String materialNumberDefault = (String){ .chars = "0", .length = 2 };
String renderQueueDefault = (String){ .chars = "2000", .length = 5 };
String offsetDefault = (String){ .chars = "(0.0, 0.0)", .length = 11 };
String colorDefault = (String){ .chars = "(1.0, 1.0, 1.0, 1.0)", .length = 21 };

// checks if some property has its default value
bool PropertyHasDefaultValue(KeyValue property)
{
	String key = property.key;
	String value = property.value;
	
	// textures and floats have no default value and i dont know what the default emissiveColor is
	// TODO: find the default emissive colour
	if (PropertyIsTexture(property) == true || PropertyIsFloat(property) == true || StringsEqual(key, emissiveColor) == true)
	{
		return false;
	}
	// the default of all properties is the same (0.0, 0.0)
	if (PropertyIsOffset(property) == true)
	{
		if (StringsEqual(value, offsetDefault) == true)
		{
			return true;
		}
		
		return false;
	}
	if (StringsEqual(key, renderType) == true)
	{
		if (StringsEqual(value, renderTypeDefault) == true)
		{
			return true;
		}
		
		return false;
	}
	if (StringsEqual(key, materialNumber) == true)
	{
		if (StringsEqual(value, materialNumberDefault) == true)
		{
			return true;
		}
		
		return false;
	}
	if (StringsEqual(key, renderQueue) == true)
	{
		if (StringsEqual(value, renderQueueDefault) == true)
		{
			return true;
		}
		
		return false;
	}
	if (StringsEqual(key, color) == true)
	{
		if (StringsEqual(value, colorDefault) == true)
		{
			return true;
		}
		
		return false;
	}
	
	printf("unknown property: %s, with value: %s\n", key.chars, value.chars);
	return false;
}

void BlankTextureSet(TextureSet * textureSet)
{
	textureSet->name = (String){ .chars = NULL, .length = 0 };
	textureSet->title = (String){ .chars = NULL, .length = 0 };
	textureSet->recolourable = false;
	textureSet->shaderType = UNKNOWN;
	textureSet->meshNames = (StringArray){ .strings = NULL, .length = 0 };
	textureSet->properties = (KeyValueArray){ .keysValues = NULL, .length = 0 };
}

// creates a texture set from a material
void MaterialToTextureSet(ModelMaterial material, PartInformation part, TextureSet * textureSet)
{
	BlankTextureSet(textureSet);
	
	textureSet->shaderName = material.shaderName;
	
	// find the names of the meshes that use the material
	for (int meshIndex = 0; meshIndex < material.meshIds.length; meshIndex ++)
	{
		int meshId = material.meshIds.ints[meshIndex];
		ModelMesh * mesh = GetMeshWithId(part, meshId);
		
		if (mesh == NULL)
		{
			continue;
		}
		
		textureSet->meshNames.length += 1;
		textureSet->meshNames.strings = realloc(textureSet->meshNames.strings, sizeof(String) * textureSet->meshNames.length);
		textureSet->meshNames.strings[textureSet->meshNames.length - 1] = mesh->name;
	}
	
	// look through the properties, parse them and add them to the apropriate array
	for (int index = 0; index < material.properties.length; index ++)
	{
		KeyValue property = material.properties.keysValues[index];
//		printf("key: %s, value: %s\n", property.key.chars, property.value.chars);
		
		// ignore default properties
		if (PropertyHasDefaultValue(property) == true)
		{
			continue;
		}
		else if (PropertyIsIrrelevant(property) == true)
		{
			printf("property: %s, has non default value: %s, but is probably irrelevant, skipping.\n", property.key.chars, property.value.chars);
			continue;
		}
		else if (PropertyIsKnown(property) == true)
		{
			textureSet->properties.length += 1;
			textureSet->properties.keysValues = realloc(textureSet->properties.keysValues, sizeof(KeyValue) * textureSet->properties.length);
			textureSet->properties.keysValues[textureSet->properties.length - 1] = property;
			continue;
		}
		else
		{
			printf("WARNING: property: %s, with value: %s, is unrecognised.\n", property.key.chars, property.value.chars);
			textureSet->properties.length += 1;
			textureSet->properties.keysValues = realloc(textureSet->properties.keysValues, sizeof(KeyValue) * textureSet->properties.length);
			textureSet->properties.keysValues[textureSet->properties.length - 1] = property;
		}
	}
}

// create a texture switch and give it a default and recolour texture set for each relevant material
void CreateTextureSets(MaterialArray materials, PartInformation part, TextureSwitchArray * textureSwitches)
{
	// an array of materials that have already been processed, used to detect duplicte materials
	MaterialArray processedMaterials = (MaterialArray){ .materials = NULL, .length = 0 };
	
	// for each material the part has
	for (int index = 0; index < materials.length; index ++)
	{
		ModelMaterial material = materials.materials[index];
		// check if the material is relevant to recolouring
		ShaderType shaderType = GetShaderType(material);
		
		if (shaderType == IRRELEVANT)
		{
			continue;
		}
		
		if (IndexOfMaterial(processedMaterials, material) != -1)
		{
			// duplicate material detected, skip
			continue;
		}
		
		// this material is relevant, create a new texture switch, with a default and recolour texture set
		TextureSwitch * textureSwitch = malloc(sizeof(TextureSwitch));
		*textureSwitch = (TextureSwitch){ .name = (String){ .chars = "<NAME>", .length = 7 }, .currentTextureSet = NULL, .textureSets = (TextureSetArray){ .textureSets = NULL, .length = 0 }};
		// TODO: give the texture switches better names
		textureSwitch->textureSets.textureSets = malloc(sizeof(TextureSet) * 2);
		textureSwitch->textureSets.length = 2;
		
		TextureSet * defaultTextureSet = &textureSwitch->textureSets.textureSets[0];
		TextureSet * recolourTextureSet = &textureSwitch->textureSets.textureSets[1];
		
		MaterialToTextureSet(material, part, defaultTextureSet);
		MaterialToTextureSet(material, part, recolourTextureSet);
		
		defaultTextureSet->recolourable = false;
		recolourTextureSet->recolourable = true;
		
		// set the texture sets names as the material name plus _default / _recolour
		ConcatenateStrings(material.materialName, (String){ .chars = "_default", .length = 9 }, &defaultTextureSet->name);
		ConcatenateStrings(material.materialName, (String){ .chars = "_recolour", .length = 10 }, &recolourTextureSet->name);
		
		// set the in game titles
		recolourTextureSet->title = (String){ .chars = "Bob Ross Paints", .length = 16 };
		defaultTextureSet->title = (String){ .chars = "Default", .length = 8 };
		
		// the default texture set keeps its original shader
		defaultTextureSet->shaderType = shaderType;
		// the recolour gets a different shader depending on the type of the original shader
		recolourTextureSet->shaderType = SelectRecolourShaderType(shaderType);
		RecolourShaderTypeName(recolourTextureSet->shaderType, &recolourTextureSet->shaderName);
		
		// set the texture switch to select the default texture set by default
		textureSwitch->currentTextureSet = defaultTextureSet;
		
		// add the material that just had texture sets made to the list of processed materials
		processedMaterials.length += 1;
		processedMaterials.materials = realloc(processedMaterials.materials, sizeof(ModelMaterial) * processedMaterials.length);
		processedMaterials.materials[processedMaterials.length - 1] = material;
		
		// add the new texture switch to the list of texture switches
		textureSwitches->length += 1;
		textureSwitches->textureSwitches = realloc(textureSwitches->textureSwitches, sizeof(TextureSwitch) * textureSwitches->length);
		textureSwitches->textureSwitches[textureSwitches->length - 1] = *textureSwitch;
	}
}

void AddTextureProperty(String * patch, KeyValue property)
{
	AddToString(patch, (String){ .chars = "\n		texture = ", .length = 14 });
	AddToString(patch, property.key);
	AddToString(patch, (String){ .chars = ",", .length = 2});
	AddToString(patch, property.value);
}

void AddFloatProperty(String * patch, KeyValue property)
{
	AddToString(patch, (String){ .chars = "\n		PROPERTY\n		{\n			name = ", .length = 27 });
	AddToString(patch, property.key);
	AddToString(patch, (String){ .chars = "\n			float = ", .length = 13 });
	AddToString(patch, property.value);
	AddToString(patch, (String){ .chars = "\n		}", .length = 5 });
}

void AddColorProperty(String * patch, KeyValue property)
{
	AddToString(patch, (String){ .chars = "\n		PROPERTY\n		{\n			name = ", .length = 27 });
	AddToString(patch, property.key);
	AddToString(patch, (String){ .chars = "\n			color = ", .length = 13 });
	
	// get rid of the ( ) around the value
	// (1.0, 1.0, 1.0, 1.0) 20
	// 1.0, 1.0, 1.0, 1.0	18
	String cleanedValue = (String){ .chars = malloc(sizeof(char) * 18), .length = 18 };
	memcpy(cleanedValue.chars, &property.value.chars[1], sizeof(char) * cleanedValue.length);
	
	AddToString(patch, cleanedValue);
	AddToString(patch, (String){ .chars = "\n		}", .length = 5 });
	
	free(cleanedValue.chars);
}

// add the properties of a material to the default patch
void AddDefaultProperties(TextureSet textureSet, String * patch)
{
	// depending on the property type add them correctly
	for (int index = 0; index < textureSet.properties.length; index ++)
	{
		KeyValue property = textureSet.properties.keysValues[index];
		
		if (PropertyHasDefaultValue(property) == true)
		{
			// skip adding properties that have their default values
			continue;
		}
		else if (PropertyIsIrrelevant(property) == true)
		{
			printf("property: %s, has non default value: %s, but is probably irrelevant, skipping.\n", property.key.chars, property.value.chars);
			continue;
		}
		else if (PropertyIsTexture(property) == true)
		{
			AddTextureProperty(patch, property);
			continue;
		}
		else if (PropertyIsFloat(property) == true)
		{
			AddFloatProperty(patch, property);
			continue;
		}
		else if (PropertyIsColor(property) == true)
		{
			AddColorProperty(patch, property);
			continue;
		}
		else if (PropertyIsVector(property) == true)
		{
			// TODO: properly implement this
			printf("property type: %s is a vector, i'm not programmed to deal with that yet, skipping\n", property.key.chars);
			continue;
		}
		else if (PropertyIsOffset(property) == true)
		{
			// TODO: properly implement this
			printf("property type: %s is an offset, i'm not programmed to deal with that yet, skipping\n", property.key.chars);
			continue;
		}
		
		printf("WARNING: property type: %s is unknown, skipping\n", property.key.chars);
	}
}

// adds properties to the recolour patch depending on what the properties of the material are
void AddRecolourProperties(TextureSet textureSet, String * patch)
{
	// we only care about texture properties
	// _MainTex and _BumpMap should stay the same as the original material
	// if the shader is Specular or Specular (Mapped) then _SpecGlossMap should use the _MainTex (for specular) / _SpecMap (for specular mapped)
	// _Emission not sure about this one, may or may not need a custom texture is the shader is emissive, for now use _Emissive
	// _MaskTex this is where the recolour mask should go
	
	// TU shaders also have vectors:
	// _DiffuseNorm
	// _SpecularNorm
	// _SmoothnessNorm
	// TODO: automatically calculate vectors
	
	// all properties that are not textures can probably be ignored
	for (int index = 0; index < textureSet.properties.length; index ++)
	{
		KeyValue property = textureSet.properties.keysValues[index];
		
		if (StringsEqual(property.key, mainTex) == true)
		{
			// main tex
			AddTextureProperty(patch, property);
			continue;
		}
		else if (StringsEqual(property.key, bumpMap) == true)
		{
			// bump map
			AddTextureProperty(patch, property);
			continue;
		}
		else if (StringsEqual(property.key, emissive) == true)
		{
			// emissive
			AddTextureProperty(patch, (KeyValue){ .key = (String){ .chars = "_Emission", .length = 10 }, .value = property.value});
			continue;
		}
		else if (StringsEqual(property.key, metalMap) == true)
		{
			AddTextureProperty(patch, (KeyValue){ .key = metalMap, .value = property.value });
			continue;
		}
		else if (StringsEqual(property.key, specMap) == true)
		{
			// specular map
			AddTextureProperty(patch, (KeyValue){ .key = (String){ .chars = "_SpecGlossMap", .length = 13 }, .value = property.value });
			continue;
		}
		
		printf("property: %s not listed as relevant to recolouring, skipping\n", property.key.chars);
	}
	
	// tu legacy / specular shaders need a specular map, and tu metallic needs a metal map, if there is no seperate texture for these the _maintex can be used
	// tu diffuse indicates that the original material has no spec / metal map and it is not built into mainTex, so a new one must be make
	if (IndexOfKey(textureSet.properties, 0, specMap) == -1)
	{
		// if no spec map is found
		if (textureSet.shaderType != TU_DIFFUSE)
		{
			// and the material has spec / metal built into mainTex
			String mainTexValue = (String){ .chars = NULL, .length = 0 };
			ValueOfKey(textureSet.properties, mainTex, &mainTexValue);
			if (textureSet.shaderType == TU_SPECULAR || textureSet.shaderType == TU_TRANSPARENT_SPECULAR || textureSet.shaderType == TU_LEGACY || textureSet.shaderType == TU_TRANSPARENT_LEGACY)
			{
				AddTextureProperty(patch, (KeyValue){ .key = (String){ .chars = "_SpecGlossMap", .length = 14 }, .value = mainTexValue });
			}
			else if (textureSet.shaderType == TU_METALLIC || textureSet.shaderType == TU_TRANSPARENT_METALLIC)
			{
				AddTextureProperty(patch, (KeyValue){ .key = (String){ .chars = "_MetallicGlossMap", .length = 18 }, .value = mainTexValue });
			}
			free(mainTexValue.chars);
		}
		else
		{
			// TODO: a spec map is needed but not available, one needs to be created
			AddTextureProperty(patch, (KeyValue){ .key = (String){ .chars = "_SpecGlossMap", .length = 14 }, .value = (String){ .chars = "<SPEC_MAP>", .length = 11 } });
		}
	}
	
	// add the _MaskTex
	AddToString(patch, (String){ .chars = "\n		texture = _MaskTex,<RECOLOURING_MASK>\n", .length = 42 });
	// add the vectors
	AddToString(patch, (String){ .chars = "\n		vector = _DiffuseNorm,x,y,z\n		vector = _SmoothnessNorm,x,y,z\n		vector = _SpecularNorm,x,y,z", .length = 95 });
}

void AddTextureSet(String * patch, TextureSet textureSet, String template, String modName)
{
	// copy the template default texture set
	AddToString(patch, (String){ .chars = "+KSP_TEXTURE_SET[", .length = 18 });
	AddToString(patch, template);
	// set mod requirements
	AddToString(patch, (String){ .chars = "]:NEEDS[TexturesUnlimited&", .length = 27 });
	AddToString(patch, modName);
	AddToString(patch, (String){ .chars = "]\n{\n	@name = ", .length = 14 });
	AddToString(patch, textureSet.name);
	AddToString(patch, (String){ .chars = "\n	@title = ", .length = 12 });
	AddToString(patch, textureSet.title);
	AddToString(patch, (String){ .chars = "\n	@MATERIAL\n	{\n		@shader = ", .length = 28 });
	AddToString(patch, textureSet.shaderName);
	AddToString(patch, (String){ .chars = "\n", .length = 2 });
	
	// add the meshes
	for (int meshIndex = 0; meshIndex < textureSet.meshNames.length; meshIndex ++)
	{
		String meshName = textureSet.meshNames.strings[meshIndex];
	
		AddToString(patch, (String){ .chars = "\n		mesh = ", .length = 11 });
		AddToString(patch, meshName);
	}
	
	AddToString(patch, (String){ .chars = "\n", .length = 2 });
}

void AddAddTextureSetToSwitcher(String * patch, int switcherIndex, String textureSetName)
{
	AddToString(patch, (String){ .chars = "\n	@MODULE[KSPTextureSwitch],", .length = 29 });
	// add the index that indicates which texture switcher to edit
	String indexString = (String){ .chars = NULL, .length = 0 };
	IntToString(switcherIndex, &indexString);
	AddToString(patch, indexString);
	free(indexString.chars);
	
	AddToString(patch, (String){ .chars = "\n	{\n		textureSet = ", .length = 20 });
	AddToString(patch, textureSetName);
	AddToString(patch, (String){ .chars = "\n	}", .length = 4 });
}

void AddTextureSwitch(String * patch, TextureSwitch textureSwitch)
{
	AddToString(patch, (String){ .chars = "	MODULE\n	{\n		name = KSPTextureSwitch\n		sectionName = ", .length = 54 });
	AddToString(patch, textureSwitch.name);
	AddToString(patch, (String){ .chars = "\n		currentTextureSet = ", .length = 24 });
	AddToString(patch, textureSwitch.currentTextureSet->name);
	AddToString(patch, (String){ .chars = "\n		textureSet = ", .length = 17 });
	AddToString(patch, textureSwitch.currentTextureSet->name);
	AddToString(patch, (String){ .chars = "\n	}\n", .length = 5 });
}

void AddEditPart(String * patch, String partName, String order, String modName)
{
	// edit the part
	AddToString(patch, (String){ .chars = "@PART[", .length = 7 });
	AddToString(patch, partName);
	// set the orderings
	AddToString(patch, (String){ .chars = "]:FOR[", .length = 7 });
	AddToString(patch, order);
	// set the mod requirements
	AddToString(patch, (String){ .chars = "]:NEEDS[TexturesUnlimited&", .length = 27 });
	AddToString(patch, modName);
	AddToString(patch, (String){ .chars = "]\n{\n", .length = 5 });
}

// creates a patch that will replace the parts materials with texture sets controlled by texture switchers
void CreateDefaultPatch(PartInformation part, TextureSwitchArray textureSwitches, String * patch)
{
	// TODO: make a way to set mod requirements
	// TODO: make a way to set the ordering
	String order = (String){ .chars = "000_TU_Default_000", .length = 19 };
	String modName = (String){ .chars = "<MODNAME>", .length = 9 };
	
	AddEditPart(patch, *part.modules.name, order, modName);
	
	// add each texture switch
	for (int index = 0; index < textureSwitches.length; index ++)
	{
		TextureSwitch textureSwitch = textureSwitches.textureSwitches[index];
		AddTextureSwitch(patch, textureSwitch);
	}
	
	AddToString(patch, (String){ .chars = "}\n", .length = 3 });
	
	// add the default texture sets of each texture switch
	for (int textureSwitchIndex = 0; textureSwitchIndex < textureSwitches.length; textureSwitchIndex ++)
	{
		TextureSwitch textureSwitch = textureSwitches.textureSwitches[textureSwitchIndex];
		TextureSet textureSet = *textureSwitch.currentTextureSet;
		// TODO: make a way to set the default texture set to copy
		String template = (String){ .chars = "<DEFAULT_TEXTURE_SET>", .length = 22 };
		
		AddTextureSet(patch, textureSet, template, modName);
		
		// add the propeties of the textureSet to the patch
		AddDefaultProperties(textureSet, patch);
		
		AddToString(patch, (String){ .chars = "\n	}\n}\n", .length = 7 });
	}
}

// creates a patch to add recolour texture sets to the texture switches of a part
void CreateRecolourPatch(PartInformation part, TextureSwitchArray textureSwitches, String * patch)
{
	// TODO: make a way to set mod requirements
	// TODO: make a way to set the ordering
	String order = (String){ .chars = "999_TU_Recolour_000", .length = 20 };
	String modName = (String){ .chars = "<MODNAME>", .length = 10 };
	
	AddEditPart(patch, *part.modules.name, order, modName);
	
	// add the recolour texture set to the texture switchers
	for (int index = 0; index < textureSwitches.length; index ++)
	{
		TextureSwitch textureSwitch = textureSwitches.textureSwitches[index];
		String recolourTextureSetName = textureSwitch.textureSets.textureSets[1].name;
		
		AddAddTextureSetToSwitcher(patch, index, recolourTextureSetName);
	}
	
	// add the recolour button
	AddToString(patch, (String){ .chars = "\n	%MODULE[SSTURecolorGUI]\n	{\n		%name = SSTURecolorGUI\n	}\n}\n", .length = 60 });
	
	// add the recolour texture sets of each texture switch
	for (int textureSwitchIndex = 0; textureSwitchIndex < textureSwitches.length; textureSwitchIndex ++)
	{
		// add the second textrue set the switchers have (should be the recolour one)
		TextureSwitch textureSwitch = textureSwitches.textureSwitches[textureSwitchIndex];
		TextureSet textureSet = textureSwitch.textureSets.textureSets[1];
		
		String template = (String){ .chars = "<RECOLOUR_TEXTURE_SET>", .length = 23 };
		
		AddTextureSet(patch, textureSet, template, modName);
		
		// add the propeties of the material to the patch
		AddRecolourProperties(textureSet, patch);
		
		// end the texture set
		AddToString(patch, (String){ .chars = "\n	}\n}\n", .length = 7 });
	}
}

void GetPartMaterials(PartInformation * part, MaterialArray * output)
{
	for (int modelIndex = 0; modelIndex < part->models.length; modelIndex ++)
	{
		MuModel model = part->models.models[modelIndex];
		
		for (int materialIndex = 0; materialIndex < model.materials.length; materialIndex ++)
		{
			ModelMaterial material = model.materials.materials[materialIndex];
			
			output->length += 1;
			output->materials = realloc(output->materials, sizeof(ModelMaterial) * output->length);
			output->materials[output->length - 1] = material;
		}
	}
}

// the two strings represent what is in the two .cfg files that make a recolour
typedef struct RecolourConfig
{
	String defaultConfig;
	String recolourConfig;
}
RecolourConfig;

// returns a recolour config for a part
void Recolour(PartInformation * part, RecolourConfig * recolour)
{
	// for each material create a texture switcher that swaps between a default and recolour texture set
	// materials may also be duplicated if multiple of the same mesh is used in a model
	// from the texture switchers and sets create a config file that implements them
	
	// TODO: if the part has a set of variants that switch only materials this can be replicated with a texture switch
	// if the variants switch materials and meshes this can be replicated with the stock variants system using recolourable variants
	// otherwise (switching other part properties) a texture switcher must be made for each material
	
	// TODO: also check if multiple identically named meshes use different materials
	// if so: uh oh
	
	// collect the materials of all the models of the part
	MaterialArray materials = (MaterialArray){ .materials = NULL, .length = 0 };
	GetPartMaterials(part, &materials);
	
	// create a texture switch and give it a default and recolour texture set for each (relevant) material the part uses
	TextureSwitchArray textureSwitches = (TextureSwitchArray){ .length = 0, .textureSwitches = NULL };
	CreateTextureSets(materials, *part, &textureSwitches);
	
	// clear the old default config, if any
	recolour->defaultConfig = (String){ .chars = realloc(recolour->defaultConfig.chars, sizeof(char) * 0), .length = 0 };
	// create a default config patch that implements the texture switches and default texture sets
	CreateDefaultPatch(*part, textureSwitches, &recolour->defaultConfig);
	
	// clear the old recolour config, if any
	recolour->recolourConfig = (String){ .chars = realloc(recolour->recolourConfig.chars, sizeof(char) * 0), .length = 0 };
	// create a recolour patch that implements the recolour texture sets
	CreateRecolourPatch(*part, textureSwitches, &recolour->recolourConfig);
	
	printf("default patch:\n%s\n", recolour->defaultConfig.chars);
	printf("recolour patch:\n%s\n", recolour->recolourConfig.chars);
	
	free(textureSwitches.textureSwitches);
}

//█▀▄ █▀█ ▄▀█ █░█░█ █ █▄░█ █▀▀
//█▄▀ █▀▄ █▀█ ▀▄▀▄▀ █ █░▀█ █▄█

// draws an image 
void DrawImage(String textureName)
{
	Texture * image = GetTextureWithName(textureName);
	//printf("width: %d, height: %d\n", image->width, image->height);
	CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED((float)image->width), .height = CLAY_SIZING_FIXED((float)image->height) } }, .image = { .imageData = image } }) {}
}

// a button
#define BUTTON(textureName, buttonWidth, buttonHeight, ...)\
	EMBOSSED_CONTAINER																								\
	( CLAY_SIZING_FIXED(buttonWidth), CLAY_SIZING_FIXED(buttonHeight),																						\
		Clay_Color buttonDefault = RaylibToClayColour(LIGHTGRAY);													\
		Clay_Color buttonHovered = RaylibToClayColour(DARKBLUE);													\
		/*theres gotta be a -4 in CLAY_SIZING_FIXED to account for the width of the embossed container's borders*/	\
		Clay_LayoutConfig buttonLayout = (Clay_LayoutConfig){ .sizing = { .height = CLAY_SIZING_FIXED(buttonHeight - 4), .width = CLAY_SIZING_FIXED(buttonWidth - 4) }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }, .childGap = padding, .padding = CLAY_PADDING_ALL(padding) };\
																													\
		CLAY_AUTO_ID({ .backgroundColor = Clay_Hovered() ? buttonHovered : buttonDefault, .layout = buttonLayout })	\
		{																											\
			DrawImage(textureName);																					\
			__VA_ARGS__;																							\
		}																											\
	);

// draws a horizontal scrollbar
void DrawHorizontalScrollbar(Clay_ElementId scrollingContainer)
{
	// scrollbar settings
	int scrollbarHeight = 20;
	int arrowBoxSize = scrollbarHeight;	// width / height of the box that holds the arrow
	
	Clay_Color scrollbarDefault = RaylibToClayColour(LIGHTGRAY);
	Clay_Color scrollbarHovered = RaylibToClayColour(DARKBLUE);
	
	// box that holds the scrollbar
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(scrollbarHeight) } } })
	{
		// find the size of the content relative to the size of the container, use this to size the scrollbar
		Clay_ScrollContainerData scrolledContent = Clay_GetScrollContainerData(scrollingContainer);
		
		// if the content has not been created yet
		if (scrolledContent.scrollPosition == NULL)
		{
			return;
		}
		
		// find how far the content has been scrolled, use this to position the scrollbar
		float scrollX = scrolledContent.scrollPosition->x;
		float contentWidth = scrolledContent.contentDimensions.width;
		float containerWidth = scrolledContent.scrollContainerDimensions.width;
		
		// get information about the box that holds the scrollbar
		Clay_ElementData scrollbar = Clay_GetElementData((Clay_ElementId){ .id = Clay_GetOpenElementId() });
		// the width of the scrollable area
		float scrollAreaWidth = scrollbar.boundingBox.width - (2 * arrowBoxSize);
		// the proportion of the content that the containter can show
		float contentContainterWidthProportion = containerWidth / contentWidth;
		// the width of the scrollbar, capped to the width of the scrolling area
		float scrollbarWidth = scrollAreaWidth * MinFloat(contentContainterWidthProportion, 1.0f);
		
		// position the scrollbar
		// the maximum scroll the content can have
		float maxContentScroll = containerWidth - contentWidth;
		// the proportion (0 - 1) of the max scroll that the content has been scrolled
		float contentScrollProportion = scrollX / (floatImprecisionBuffer + maxContentScroll);
		// the furthest to the right the scroll bar can be positioned
		float maxScrollbarXPosition = scrollAreaWidth - scrollbarWidth;
		// the x position of the top of the scrollbar
		float scrollbarX = maxScrollbarXPosition * contentScrollProportion;
		
		//printf("scroll bar x: %f, scroll bar width: %f\n", scrollbarX, scrollbarWidth);
		
		// left side arrow
		BUTTON(((String){ .chars = "arrow-left.png", .length = 15 }), arrowBoxSize, arrowBoxSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? scrolledContent.scrollPosition->x += 5 : 0));
		
		// scrolling area
		CLAY_AUTO_ID({ .backgroundColor = RaylibToClayColour(LIGHTGRAY), .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .clip = { .vertical = false, .horizontal = true, .childOffset = { .x = scrollbarX, .y = 0 } } })
		{
			// if the area is hovered and click is held, scroll whatever scrolling container the scrollbar is connected to to wherever was clicked
			if (Clay_Hovered() == true && IsMouseButtonDown(0) == true)
			{
				// find where on the scroll bar (proportionally) was clicked
				float mouseX = GetMousePosition().x;
				Clay_ElementData scrollArea = Clay_GetElementData((Clay_ElementId){ .id = Clay_GetOpenElementId() });
				// the scroll bar should move to be centred on the mouse when clicked,
				// therefore only a section at the middle of the scroll area where the scroll bar can be centred and not hit the ends of the scroll area is considered to represent the whole scroll area
				// clicking outside of that area will count as scrolling all the way to the top / bottom
				float scrollbarCentringBuffer = scrollbarWidth / 2;
				float scrollAreaMin = scrollArea.boundingBox.x + scrollbarCentringBuffer;
				float scrollAreaMax = scrollArea.boundingBox.x + scrollAreaWidth - scrollbarCentringBuffer;
				float scrollAreaLength = scrollAreaMax - scrollAreaMin;
				
				// how far down the scroll area was clicked
				float relativeMouseX = mouseX - scrollAreaMin;
				// 0 - 1 representing how far along the scroll area was clicked
				float proportionalMouseX = relativeMouseX / scrollAreaLength;
				// find how much scrolling this corresponds to in the scrollable content
				float contentScroll = maxContentScroll * proportionalMouseX;
				//printf("content scroll %f\n", contentScroll);
				// set the scroll of the content
				scrolledContent.scrollPosition->x = contentScroll;
			}
			
			// draw the scrolling bit of the scrollbar
			EMBOSSED_CONTAINER(CLAY_SIZING_FIXED(scrollbarWidth), CLAY_SIZING_FIXED(scrollbarHeight), CLAY_AUTO_ID({ .backgroundColor = (Clay_Hovered() == true ? scrollbarHovered : scrollbarDefault), .layout = { .sizing = { .width = CLAY_SIZING_FIXED(scrollbarWidth - 4), .height = CLAY_SIZING_FIXED(scrollbarHeight - 4) } } }));
		}
		
		// right side arrow
		BUTTON(((String){ .chars = "arrow-right.png", .length = 16 }), arrowBoxSize, arrowBoxSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? scrolledContent.scrollPosition->x -= 5 : 0));
	}
}

// draws a vertical scrollbar
void DrawVerticalScrollbar(Clay_ElementId scrollingContainer)
{
	// scrollbar settings
	int scrollbarWidth = 20;
	int arrowBoxSize = scrollbarWidth;
	
	Clay_Color scrollbarDefault = RaylibToClayColour(LIGHTGRAY);
	Clay_Color scrollbarHovered = RaylibToClayColour(DARKBLUE);
	
	// box that holds the scrollbar
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(scrollbarWidth), .height = CLAY_SIZING_GROW() } } })
	{
		// find the size of the content relative to the size of the container, use this to size the scrollbar
		Clay_ScrollContainerData scrolledContent = Clay_GetScrollContainerData(scrollingContainer);
		
		// if the content has not been created yet
		if (scrolledContent.scrollPosition == NULL)
		{
			return;
		}
		
		// find how far the content has been scrolled, use this to position the scrollbar
		float scrollY = scrolledContent.scrollPosition->y;
		float contentHeight = scrolledContent.contentDimensions.height;
		float containerHeight = scrolledContent.scrollContainerDimensions.height;
		
		// get information about the box that holds the scrollbar
		Clay_ElementData scrollbar = Clay_GetElementData((Clay_ElementId){ .id = Clay_GetOpenElementId() });
		// the height of the scrollable area
		float scrollAreaHeight = scrollbar.boundingBox.height - (2 * arrowBoxSize);
		//printf("scroll area height: %f\n", scrollAreaHeight);
		// the proportion of the content that the containter can show
		float contentContainterHeightProportion = containerHeight / contentHeight;
		// the height of the scrollbar, capped to the height of the scrolling area
		float scrollbarHeight = scrollAreaHeight * MinFloat(contentContainterHeightProportion, 1.0f);
		
		// position the scrollbar
		// the maximum scroll the content can have
		float maxContentScroll = containerHeight - contentHeight;
		// the proportion (0 - 1) of the max scroll that the content has been scrolled
		float contentScrollProportion = scrollY / (floatImprecisionBuffer + maxContentScroll);
		// the furthest down the scroll bar can be positioned
		float maxScrollbarYPosition = scrollAreaHeight - scrollbarHeight;
		// the y position of the top of the scrollbar
		float scrollbarY = maxScrollbarYPosition * contentScrollProportion;
		
		//printf("scroll y: %f, content height: %f, container height: %f\n", scrollY, contentHeight, containerHeight);
		//printf("scrollbar height: %f, scrollbar y: %f\n", scrollbarHeight, scrollbarY);
		
		// top arrow
		BUTTON(((String){ .chars = "arrow-up.png", .length = 13 }), arrowBoxSize, arrowBoxSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? scrolledContent.scrollPosition->y += 5 : 0));
		
		// scrolling area
		CLAY_AUTO_ID({ .backgroundColor = RaylibToClayColour(LIGHTGRAY), .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .clip = { .vertical = true, .horizontal = false, .childOffset = { .x = 0, .y = scrollbarY } } })
		{
			// if the area is hovered and click is held, scroll whatever scrolling container the scrollbar is connected to to wherever was clicked
			if (Clay_Hovered() == true && IsMouseButtonDown(0) == true)
			{
				// find where on the scroll bar (proportionally) was clicked
				float mouseY = GetMousePosition().y;
				Clay_ElementData scrollArea = Clay_GetElementData((Clay_ElementId){ .id = Clay_GetOpenElementId() });
				// the scroll bar should move to be centred on the mouse when clicked,
				// therefore only a section at the middle of the scroll area where the scroll bar can be centred and not hit the ends of the scroll area is considered to represent the whole scroll area
				// clicking outside of that area will count as scrolling all the way to the top / bottom
				float scrollbarCentringBuffer = scrollbarHeight / 2;
				float scrollAreaMin = scrollArea.boundingBox.y + scrollbarCentringBuffer;
				float scrollAreaMax = scrollArea.boundingBox.y + scrollAreaHeight - scrollbarCentringBuffer;
				float scrollAreaLength = scrollAreaMax - scrollAreaMin;
				
				// how far down the scroll area was clicked
				float relativeMouseY = mouseY - scrollAreaMin;
				// 0 - 1 representing how far along the scroll area was clicked
				float proportionalMouseY = relativeMouseY / scrollAreaLength;
				// find how much scrolling this corresponds to in the scrollable content
				float contentScroll = maxContentScroll * proportionalMouseY;
				//printf("content scroll %f\n", contentScroll);
				// set the scroll of the content
				scrolledContent.scrollPosition->y = contentScroll;
			}
			
			// draw the scrolling bit of the scrollbar
			EMBOSSED_CONTAINER(CLAY_SIZING_FIXED(scrollbarWidth), CLAY_SIZING_FIXED(scrollbarHeight), CLAY_AUTO_ID({ .backgroundColor = (Clay_Hovered() == true ? scrollbarHovered : scrollbarDefault), .layout = { .sizing = { .width = CLAY_SIZING_FIXED(scrollbarWidth - 4), .height = CLAY_SIZING_FIXED(scrollbarHeight - 4) } } }));
		}
		
		// bottom arrow
		BUTTON(((String){ .chars = "arrow-down.png", .length = 15 }), arrowBoxSize, arrowBoxSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? scrolledContent.scrollPosition->y -= 5 : 0));
	}
}

// sets current directory to the KSP install folder
void SetSelectedDirectory(String ** directory)
{
	// should start at the folder called "Kerbal Space Program"
	
	int initialDirectorylength = 7; 			// string plus null byte
	const char * initialDirectory = "../../";	
	
	// this should exist for the whole lifetime of the program so dont free
	String * initial = malloc(sizeof(String));
	*initial = (String){ .length = initialDirectorylength, .chars = malloc(sizeof(char) * initialDirectorylength)};
	memcpy(initial->chars, initialDirectory, sizeof(char) * initialDirectorylength);
	
	*directory = initial;
}

typedef enum DirectoryType
{
	FOLDER,
	CONFIG,
	TEXTURE,
	MODEL,
	TEXT,
	DOT,
	DOTDOT,
	OTHER
}
DirectoryType;

DirectoryType FindDirectoryType(String directoryName)
{
	if (IsDot(directoryName) == true)
	{
		return DOT;
	}
	
	if (IsDotDot(directoryName) == true)
	{
		return DOTDOT;
	}
	
//	printf("directory name: ");
//	BetterPrint(directoryName);
//	printf("directory length: %d\n", directoryName.length);
	
	// find if the directory name has a .
	int dotIndex = IndexOfChar(directoryName, 0, '.');
	
	if (dotIndex == -1)
	{
		return FOLDER;
	}
	
	// extract the directories extenstion
	String extension = (String){ .chars = NULL, .length = 0 };
	ExtractString(directoryName, &extension, dotIndex, directoryName.length - 1);
	
	// ......V... 6
	// cheese.png 10
//	printf("directory extension: ");
//	BetterPrint(extension);
	
	// config file extension
	String cfg = (String){ .chars = ".cfg", .length = 5 };
	// ksp's model format
	String mu = (String){ .chars = ".mu", .length = 4 };
	// text file formats
	String txt = (String){ .chars = ".txt", .length = 5 };
	String md = (String){ .chars = ".md", .length = 4 };
	// texture file formats
	String png = (String){ .chars = ".png", .length = 5 };
	String dds = (String){ .chars = ".dds", .length = 5 };
	
	if (StringsEqual(extension, cfg) == true)
	{
		return CONFIG;
	}
	else if (StringsEqual(extension, mu) == true)
	{
		return MODEL;
	}
	else if (StringsEqual(extension, png) == true || StringsEqual(extension, dds) == true)
	{
		return TEXTURE;
	}
	else if (StringsEqual(extension, txt) == true || StringsEqual(extension, md) == true)
	{
		return TEXT;
	}
	
	free(extension.chars);
	return OTHER;
}

void DrawDirectoryType(DirectoryType type)
{
	if (type == FOLDER)
	{
		DrawImage((String){ .chars = "Folder.png", .length = 11 });
	}
	else if (type == CONFIG)
	{
		DrawImage((String){ .chars = "FileConfig.png", .length = 15 });
	}
	else if (type == TEXT)
	{
		DrawImage((String){ .chars = "FileText.png", .length = 13 });
	}
	else if (type == TEXTURE)
	{
		DrawImage((String){ .chars = "FileTexture.png", .length = 16 });
	}
	else if (type == MODEL)
	{
		DrawImage((String){ .chars = "FileModel.png", .length = 14 });
	}
	else if (type == DOTDOT)
	{
		DrawImage((String){ .chars = "FileExit.png", .length = 13 });
	}
	else
	{
		DrawImage((String){ .chars = "FileUnknown.png", .length = 16 });
	}
}

// draw the enteries of the selected directory and manage what happens if a directory is clicked
void DirectoryNavigator(PartInformation * part)
{
	// the path to the currently selected directory in the file browser
	// its a pointer so it can be static
	static String * selectedDirectoryPath = NULL;
	// set the selectedDirectoryPath the first time this function runs
	if (selectedDirectoryPath == NULL)
	{
		SetSelectedDirectory(&selectedDirectoryPath);
	}

	// settings
	Clay_Color directoryBackground = RaylibToClayColour(BLANK);
	Clay_Color directoryBackgroundHovered = RaylibToClayColour(DARKBLUE);
	Clay_Color directoryText = textColour;
	Clay_Color directoryTextHovered = RaylibToClayColour(WHITE);
	int directoryNavigatorWidth = 200;
	
	DIR * selectedDirectory = opendir(selectedDirectoryPath->chars);
	if (selectedDirectory == NULL)
	{
		int err = errno;
		printf("ERROR: Could not open: %s because: %s\n", selectedDirectoryPath->chars, strerror(err));
		return;
	}
	
	// go back to the first item in the selectedDirectory
	rewinddir(selectedDirectory);
	struct dirent * entry = readdir(selectedDirectory);
	
	EMBOSSED_CONTAINER_INVERSE
	( CLAY_SIZING_GROW(), CLAY_SIZING_GROW(),
		// create a box to store the horizontal scroll bar
		CLAY_AUTO_ID({ .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			// create a scrolling box to display the directories entries in
			CLAY(CLAY_ID("DirectoryNavigator"), { .layout = { .sizing = { .width = CLAY_SIZING_FIXED(directoryNavigatorWidth), .height = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding, .layoutDirection = CLAY_TOP_TO_BOTTOM}, .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() } })
			{
				// draw all the entries in the directory
				while (entry != NULL)
				{
					// draw a button with the name of the directory entry
					CLAY_AUTO_ID({ .backgroundColor = Clay_Hovered() ? directoryBackgroundHovered : directoryBackground, .layout = { .childGap = padding, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })
					{
						// copy the directories name out of the dirent and add a null terminator
						String directoryName = (String){ .length = strlen(entry->d_name), .chars = malloc(strlen(entry->d_name)) };
						memcpy(directoryName.chars, entry->d_name, sizeof(char) * directoryName.length);
						GarunteeNullTerminator(&directoryName);
						
						// find the type of the directory from its name
						DirectoryType directoryType = FindDirectoryType(directoryName);
						DrawDirectoryType(directoryType);
						
						// draw the name of the directory
						CLAY_TEXT(MyToClayString(directoryName), { .textColor = Clay_Hovered() ? directoryTextHovered : directoryText, .fontSize = fontSize, .fontId = 0 });
						
						// if this directory has been clicked on navigate to this directory
						if (Clay_Hovered() && IsMouseButtonPressed(0))
						{
							NavigateDirectory(selectedDirectoryPath, directoryName, part);
						}
						
						free(directoryName.chars);
					}
					// read the next entry in the directory
					entry = readdir(selectedDirectory);
				}
				
				closedir(selectedDirectory);
			}
			
			Clay_String navigatorName = MyToClayString((String){ .chars = "DirectoryNavigator", .length = 19 });
			DrawHorizontalScrollbar(Clay_GetElementId(navigatorName));
		}
		
		Clay_String navigatorName = MyToClayString((String){ .chars = "DirectoryNavigator", .length = 19 });
		DrawVerticalScrollbar(Clay_GetElementId(navigatorName));
	);
}

// copys a string to the clipboard if this element is clicked on
void CopyIfClicked(String string)
{
	// if the value is clicked on, copy it to the clipboard
	if (Clay_Hovered() && IsMouseButtonDown(0))
	{
		SetClipboardText(string.chars);
	}
}

void DrawPropertyValue(String property, String value, String seperator, int valueMaxLength)
{
	// settings
	Clay_Color valueHovered = RaylibToClayColour(SKYBLUE);
	Clay_Color valueDefault = RaylibToClayColour(BLANK);
	
	(void)seperator;
	
	// create a box that is a single line tall
	// inside this will be the property and another box, with children arranged right to get the value on the other side of the box 
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		// draw the property and seperator
		CLAY_TEXT(MyToClayString(property), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
		CLAY_TEXT(MyToClayString(seperator), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
		
		// create a child box that is right aligned to get the value on the right of the box
		CLAY_AUTO_ID({ .cornerRadius = CLAY_CORNER_RADIUS(2), .backgroundColor = Clay_Hovered() ? valueHovered : valueDefault, .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			// copy the value to the clipboard if it is clicked
			CopyIfClicked(value);
			
			// draw the value
			String clampedValue = { .chars = NULL, .length = 0 };
			ClampStringLength(value, &clampedValue, valueMaxLength);
			CLAY_TEXT(MyToClayString(clampedValue), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			free(clampedValue.chars);
		}
	}
}

// checks if a module's type is relevant 
bool ModuleHasRelevantType(Module module)
{
	String partType = (String){ .chars = "PART", .length = 5 };
	String modelType = (String){ .chars = "MODEL", .length = 6 };
	String variantType = (String){ .chars = "VARIANT", .length = 8 };
	String gameobjectsType = (String){ .chars = "GAMEOBJECTS", .length = 12 };
	String subtypeType = (String){ .chars = "SUBTYPE", .length = 8 };
	String textureType = (String){ .chars = "TEXTURE", .length = 8 };
	String transformType = (String){ .chars = "TRANSFORM", .length = 10 };
	String shabbyType = (String){ .chars = "SHABBY_MATERIAL_REPLACE", .length = 24 };
	int relevantTypeCount = 8;
	
	StringArray relevantTypes = (StringArray){ .strings = malloc(sizeof(String) * relevantTypeCount), .length = relevantTypeCount };
	relevantTypes.strings[0] = partType;
	relevantTypes.strings[1] = modelType;
	relevantTypes.strings[2] = variantType;
	relevantTypes.strings[3] = gameobjectsType;
	relevantTypes.strings[4] = subtypeType;
	relevantTypes.strings[5] = textureType;
	relevantTypes.strings[6] = transformType;
	relevantTypes.strings[7] = shabbyType;
	
	String moduleType = module.type;
	
	if (StringEqualToAny(moduleType, relevantTypes) == true)
	{
//		printf("relevant module type: ");
//		BetterPrint(moduleType);
		free(relevantTypes.strings);
		return true;
	}
	
	free(relevantTypes.strings);
	return false;
}

// checks if a modules name is relevant
bool ModuleHasRelevantName(Module module)
{
	String b9SwitchName = (String){ .chars = "ModuleB9PartSwitch", .length = 19 };
	String variantsName = (String){ .chars = "ModulePartVariants", .length = 19 };
	
	String moduleName = *module.name;
	
	if (StringsEqual(moduleName, b9SwitchName) == true || StringsEqual(moduleName, variantsName) == true)
	{
		return true;
	}
	
	return false;
}

// checks if a module is relevant to creating a recolour
bool ModuleIsRelevant(Module module)
{
	if (ModuleHasRelevantType(module) == true)
	{
		return true;
	}
	if (ModuleHasRelevantName(module) == true)
	{
		return true;
	}
	return false;
}

// draws a module
void DrawModule(Module module, bool onlyRelevantModules)
{
	// draw the modules type
	CLAY_TEXT(MyToClayString(module.type), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
	
	// only draw the properties of relevant modules
	if (onlyRelevantModules == true && ModuleIsRelevant(module) == false)
	{
		return;
	}
	
	// draw each of the modules properties
	for (int propertyIndex = 0; propertyIndex < module.properties.length; propertyIndex ++)
	{
		KeyValue property = module.properties.keysValues[propertyIndex];
		// draw the property
		int valueMaxLength = 30;
		DrawPropertyValue(property.key, property.value, (String){ .chars = " = ", .length = 4 }, valueMaxLength);
	}
	
	// draw an arrow connecting to the verticall line drawn by the parent
	// TODO: redo this
//	DrawArrow((Vector2){ .x = moduleXPosition - halfChildHorizontalOffset, .y = moduleYPosition + halfHeight }, (Vector2){ .x = moduleXPosition, .y = moduleYPosition + halfHeight }, 1, RED);
}

// draws the module tree, depth first
void DrawModuleTree(Module module, bool onlyRelevantModules)
{
	// draw the module,
	// create a box below the module to put children in
	// draw each child in a stack a bit to the right in that box
	int childOffset = 50;
	
	// if the top level part module has no type no part has been selected
	if (module.parent == NULL && StringsEqual(module.type, (String) { .chars = NULL, .length = 0 }))
	{
		return;
	}
	
	CLAY_AUTO_ID({ .layout = { .padding = CLAY_PADDING_ALL(padding) } })
	{
		CLAY_AUTO_ID({ .cornerRadius = CLAY_CORNER_RADIUS(cornerRadius), .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = CLAY_PADDING_ALL(padding) } })
		{
			// draw this module
			DrawModule(module, onlyRelevantModules);
		}
	}
	
	// if there are no children do not create containers for one
	if (module.children.length == 0)
	{
		return;
	}
	
	// create the boxes for children
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		// filler box to move the childen over a bit
		CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(childOffset), .height = CLAY_SIZING_GROW() } } }) {}
		// box to put the children in
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM } })
		{
			for (int index = 0; index < module.children.length; index ++)
			{
				// draw the childen
				DrawModuleTree(module.children.modules[index], onlyRelevantModules);
			}
		}
	}
}

void DrawPartModules(Module module, bool onlyRelevantModules)
{
	EMBOSSED_CONTAINER_INVERSE
	( CLAY_SIZING_GROW(), CLAY_SIZING_GROW(),
		// create a box to store the horizontal scroll bar
		CLAY_AUTO_ID({ .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			// a scrolling box to put the module tree in
			CLAY(CLAY_ID("ModuleTree"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() } })
			{
				// draw the module tree, starting at this module
				DrawModuleTree(module, onlyRelevantModules);
			}
			
			Clay_String moduleTreeName = MyToClayString((String){ .chars = "ModuleTree", .length = 11 });
			DrawHorizontalScrollbar(Clay_GetElementId(moduleTreeName));
		}
		
		Clay_String moduleTreeName = MyToClayString((String){ .chars = "ModuleTree", .length = 11 });
		DrawVerticalScrollbar(Clay_GetElementId(moduleTreeName));
	);
}

// checks if a property is relevant to creating a recolour
bool MaterialPropertyIsRelevant(KeyValue property)
{
	// check if properties have their default values
	KeyValue renderTypeDefault = (KeyValue){ .key = (String){ .chars = "Render Type", .length = 12 }, .value = (String){ .chars = "MeshRenderer", .length = 13 } };
	KeyValue materialNumberDefault = (KeyValue){ .key = (String){ .chars = "Material #", .length = 11 }, .value = (String){ .chars = "0", .length = 2 } };
	KeyValue renderQueueDefault = (KeyValue){ .key = (String){ .chars = "RenderQueue", .length = 12 }, .value = (String){ .chars = "2000", .length = 5 } };
	KeyValue mainTexOffsetDefault = (KeyValue){ .key = (String){ .chars = "_MainTexOffset", .length = 15 }, .value = (String){ .chars = "(0.0, 0.0)", .length = 11 } };
	KeyValue bumpMapOffsetDefault = (KeyValue){ .key = (String){ .chars = "_BumpMapOffset", .length = 15 }, .value = (String){ .chars = "(0.0, 0.0)", .length = 11 } };
	KeyValue emissiveOffsetDefault = (KeyValue){ .key = (String){ .chars = "_EmissiveOffset", .length = 16 }, .value = (String){ .chars = "(0.0, 0.0)", .length = 11 } };
	KeyValue colourDefault = (KeyValue){ .key = (String){ .chars = "_Color", .length = 7 }, .value = (String){ .chars = "(1.0, 1.0, 1.0, 1.0)", .length = 21 } };
	KeyValue emissiveColourDefault = (KeyValue){ .key = (String){ .chars = "_EmissiveColor", .length = 15 }, .value = (String){ .chars = "(1.0, 1.0, 1.0, 0.0)", .length = 21 } };
	KeyValue specMapOffsetDefault = (KeyValue){ .key = (String){ .chars = "_SpecMapOffset", .length = 15 }, .value = (String){ .chars = "(0.0, 0.0)", .length = 11 } };
	int defaultPropertyCount = 9;
	//(KeyValue){ .key = (String){ .chars = "", .length = }, .value = (String){ .chars = "", .length = } };
	
	KeyValueArray defaultProperties = (KeyValueArray){ .keysValues = malloc(sizeof(KeyValue) * defaultPropertyCount), .length = defaultPropertyCount };
	defaultProperties.keysValues[0] = renderTypeDefault;
	defaultProperties.keysValues[1] = materialNumberDefault;
	defaultProperties.keysValues[2] = renderQueueDefault;
	defaultProperties.keysValues[3] = mainTexOffsetDefault;
	defaultProperties.keysValues[4] = bumpMapOffsetDefault;
	defaultProperties.keysValues[5] = emissiveOffsetDefault;
	defaultProperties.keysValues[6] = colourDefault;
	defaultProperties.keysValues[7] = emissiveColourDefault;
	defaultProperties.keysValues[8] = specMapOffsetDefault;
	
	// a property is relevant if it does not have its default value
	if (KeyValueEqualToAny(property, defaultProperties) == true)
	{
		free(defaultProperties.keysValues);
		return false;
	}
	
	free(defaultProperties.keysValues);
	return true;
}

// draws a single material
void DrawMaterial(ModelMaterial material, PartInformation part, bool onlyRelevantMaterials)
{
	// settings
	Clay_Color meshHovered = RaylibToClayColour(SKYBLUE);
	Clay_Color meshDefault = RaylibToClayColour(BLANK);
	
	// draw the materials name with a yellow background
	CLAY_AUTO_ID({ .cornerRadius = CLAY_CORNER_RADIUS(cornerRadius), .backgroundColor = RaylibToClayColour(YELLOW), .layout = { .padding = CLAY_PADDING_ALL(padding), .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		DrawPropertyValue((String){ .chars = "Material name: ", .length = 16 }, material.materialName, (String){ 0 }, -1);
	}
	
	// draw the shader type
	DrawPropertyValue((String){ .chars = "Shader type: ", .length = 14 }, material.shaderName, (String){ 0 }, -1);
	
	// draw the meshes
	CLAY_TEXT(MyToClayString((String){ .chars = "Meshes that use this material:", .length = 31 }), { .textColor = textColour, .fontSize = fontSize, .fontId = 0});
	CLAY_AUTO_ID({ .layout = { .padding = { .left = 3 * padding, .right = 0, .top = 0, .bottom = 0 }, .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		for (int index = 0; index < material.meshIds.length; index ++)
		{
			// highlight the mesh if it is hovered over
			CLAY_AUTO_ID({ .cornerRadius = CLAY_CORNER_RADIUS(2), .backgroundColor = Clay_Hovered() ? meshHovered : meshDefault , .layout = { .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
			{
				// get the name of the mesh
				int meshId = material.meshIds.ints[index];
				ModelMesh mesh = *GetMeshWithId(part, meshId);
				String meshName = mesh.name;
				
				// copy the mesh name to the clipboard if it is clicked
				CopyIfClicked(meshName);
				
				// draw the name of the mesh
				CLAY_TEXT(MyToClayString(meshName), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			}
		}
	}
	
	// draw the properties
	for (int index = 0; index < material.properties.length; index ++)
	{
		KeyValue property = material.properties.keysValues[index];
		
		if (onlyRelevantMaterials == true && MaterialPropertyIsRelevant(property) == false)
		{
			continue;
		}
		DrawPropertyValue(property.key, property.value, (String){ .chars = " : ", .length = 4 }, -1);
	}
}

// draws the materials of a model
void DrawMaterials(MuModel model, PartInformation part, bool onlyRelevantMaterials)
{
	CLAY_AUTO_ID({ .cornerRadius = CLAY_CORNER_RADIUS(cornerRadius), .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding } })
	{
		// draw the name of the model
		DrawPropertyValue((String){ .chars = "Model name: ", .length = 13 }, model.name, (String){ 0 }, -1);
		
		for (int index = 0; index < model.materials.length; index ++)
		{
			ModelMaterial material = model.materials.materials[index];
			DrawMaterial(material, part, onlyRelevantMaterials);
		}
	}
}

// draws an array of models
void DrawModels(ModelArray models, PartInformation part, bool onlyRelevantMaterials)
{
	EMBOSSED_CONTAINER_INVERSE
	( CLAY_SIZING_GROW(), CLAY_SIZING_GROW(),
		// create a box to store the horizontal scroll bar
		CLAY_AUTO_ID({ .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			// a scrolling box to put the models in
			CLAY(CLAY_ID("Models"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding }, .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() } })
			{
				//printf("drawing: %d models\n", models.length);
				for (int index = 0; index < models.length; index ++)
				{
					MuModel model = models.models[index];
					DrawMaterials(model, part, onlyRelevantMaterials);
				}
			}
			
			Clay_String modelsName = MyToClayString((String){ .chars = "Models", .length = 7 });
			DrawHorizontalScrollbar(Clay_GetElementId(modelsName));
		}
		
		Clay_String modelsName = MyToClayString((String){ .chars = "Models", .length = 7 });
		DrawVerticalScrollbar(Clay_GetElementId(modelsName));
	);
}

// draws the navbar and checks which buttons are pressed
void DrawNavbar(bool * onlyRelevantMaterials, bool * onlyRelevantModules, PartInformation * part, RecolourConfig * recolour)
{
	const int buttonIconSize = 48;
	CLAY(CLAY_ID("NavBar"), { .layout = { .childGap = padding, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } }, .backgroundColor = RaylibToClayColour(LIGHTGRAY) })
	{
		// buttons that turns on and off relevant modules
		BUTTON((*onlyRelevantModules == true ? (String){ .chars = "ModuleTreeRelevantOn.png", .length = 25 } : (String){ .chars = "ModuleTreeRelevantOff.png", .length = 26 }), buttonIconSize, buttonIconSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? *onlyRelevantModules = !(*onlyRelevantModules) : 0));
		
		// buttons that turns on and off relevant materials
		BUTTON((*onlyRelevantMaterials == true ? (String){ .chars = "ModelsListRelevantOn.png", .length = 25 } : (String){ .chars = "ModelsListRelevantOff.png", .length = 26 }), buttonIconSize, buttonIconSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? *onlyRelevantMaterials = !(*onlyRelevantMaterials) : 0));
		
		// button that calls Recolour if pressed
		BUTTON(((String){ .chars = "Recolour.png", .length = 13 }), buttonIconSize, buttonIconSize, ((Clay_Hovered() == true && IsMouseButtonPressed(0) == true) ? Recolour(part, recolour) : NULL));
	}
}

void DrawConfig(String config)
{
	if (config.length == 0)
	{
		return;
	}
	
	const int printIconSize = 48;
	CLAY_AUTO_ID({ .cornerRadius = cornerRadius, .layout = { .padding = CLAY_PADDING_ALL(padding), .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .backgroundColor = RaylibToClayColour(WHITE) })
	{
		// a button that if clicked will copy the config into the clipboard
		BUTTON(((String){ .chars = "Copy.png", .length = 9 }), printIconSize, printIconSize, CopyIfClicked(config));
	
		int lineStart = 0;
		int lineEnd = 0;
		
		// draw each line of text in the config
		while (true)
		{
			lineStart = lineEnd;
			lineEnd = IndexOfChar(config, lineStart + 1, '\n');
			
			// if there are no more lines
			if (lineEnd == -1)
			{
				break;
			}
			
			String line = (String){ .chars = NULL, .length = 0 };
			ExtractString(config, &line, lineStart + 1, lineEnd - 1);
			
			CLAY_TEXT(MyToClayString(line), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			free(line.chars);
		}
	}
}

void DrawRecolour(RecolourConfig recolour)
{
	EMBOSSED_CONTAINER_INVERSE
	( CLAY_SIZING_GROW(), CLAY_SIZING_GROW(),
		// create a box to store the horizontal scroll bar
		CLAY_AUTO_ID({ .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			// a scrolling box to put the configs in
			CLAY(CLAY_ID("Recolour"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding }, .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() } })
			{
				// box to change to left to right layout
				CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = padding, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })
				{
					// draw the default config
					DrawConfig(recolour.defaultConfig);
					// draw the recolour config
					DrawConfig(recolour.recolourConfig);
				}
			}
			
			Clay_String modelsName = MyToClayString((String){ .chars = "Recolour", .length = 9 });
			DrawHorizontalScrollbar(Clay_GetElementId(modelsName));
		}
		
		Clay_String modelsName = MyToClayString((String){ .chars = "Recolour", .length = 9 });
		DrawVerticalScrollbar(Clay_GetElementId(modelsName));
	);
}

//█▀▄▀█ ▄▀█ █ █▄░█
//█░▀░█ █▀█ █ █░▀█

int main(void)
{
	//-------------------------------------------------------------------------------------- Initialization
	int screenWidth = 1440;
	int screenHeight = 800;
	const char * title = "Recolour maker";
	
	// create a window
	Clay_Raylib_Initialize(screenWidth, screenHeight, title, FLAG_WINDOW_RESIZABLE);
	// load a font for clay to use
	Font fonts[1];
	fonts[0] = LoadFontEx("Roboto-Regular.ttf", 48, 0, 400);
	SetTextureFilter(fonts[0].texture, TEXTURE_FILTER_BILINEAR);
	// initialise clay
	int memoryMultiplier = 10;	// opening debug mode without this crashes the program 🦐️
	uint64_t clayRequiredMemory = memoryMultiplier * Clay_MinMemorySize();
	Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayRequiredMemory, malloc(clayRequiredMemory));
	Clay_Initialize(clayMemory, (Clay_Dimensions){ .width = GetScreenWidth(), .height = GetScreenHeight() }, (Clay_ErrorHandler){ .errorHandlerFunction = HandleClayErrors, .userData = NULL });
	Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);
	
	// load all textures in /Sprites into the texture atlas
	LoadTextureAtlas();
	LoadEmbossedTextures();
	
	// the currently selected part
	PartInformation selectedPart = { 0 };
	
	// the current recolour
	RecolourConfig recolour = (RecolourConfig){ .defaultConfig = (String){ .chars = NULL, .length = 0 }, .recolourConfig = (String){ .chars = NULL, .length = 0 } };
	
	// is debug mode on
	bool debug = false;
	
	// ui options
	bool onlyRelevantModules = false;
	bool onlyRelevantMaterials = false;
	
	SetTargetFPS(60); // target framerate
	SetExitKey(KEY_BACKSPACE); // key to close the game
	
	//-------------------------------------------------------------------------------------- Update Loop
	while (!WindowShouldClose()) // my loope ❤️😁️
	{
		float deltaTime = GetFrameTime();
		Vector2 mousePosition = GetMousePosition();
		Vector2 mouseWheelMovement = GetMouseWheelMoveV();
		
		// hold control to scroll horizontally
		if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
		{
			float x = mouseWheelMovement.x;
			mouseWheelMovement.x = mouseWheelMovement.y;
			mouseWheelMovement.y = x;
		}
		
		// hold shift to scroll faster
		float scrollMultiplier = 10;
		if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
		{
			mouseWheelMovement = Vector2Scale(mouseWheelMovement, scrollMultiplier);
		}
		
		// turn on / off debug mode
		if (IsKeyPressed(KEY_D) == true)
		{
			debug = !debug;
			Clay_SetDebugModeEnabled(debug);
		}
		
		// clay update logic
		Clay_SetLayoutDimensions((Clay_Dimensions){ .width = GetScreenWidth(), .height = GetScreenHeight() });
		Clay_SetPointerState((Clay_Vector2){ .x = mousePosition.x, .y = mousePosition.y }, IsMouseButtonDown(0));
		Clay_UpdateScrollContainers(false, (Clay_Vector2){ .x = mouseWheelMovement.x, .y = mouseWheelMovement.y }, deltaTime);
		
		// enable ui building
		Clay_BeginLayout();
		
		EMBOSSED_CONTAINER
		( CLAY_SIZING_GROW(), CLAY_SIZING_GROW(),
			// a window that fills the whole screen
			CLAY(CLAY_ID("MainWindow"), { .layout = { .childGap = padding, .padding = CLAY_PADDING_ALL(padding), .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })
			{
				// buttons to controll the program
				DrawNavbar(&onlyRelevantMaterials, &onlyRelevantModules, &selectedPart, &recolour);
				
				// holds the boxes that hold the functions of the program
				CLAY(CLAY_ID("Content"), { .layout = { .childGap = padding, .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })
				{
					// the box that holds the directory navigator
					CLAY(CLAY_ID("NavigatorBox"), { .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() } } })
					{
						// draw the directory navigator and handle clicking on directories
						DirectoryNavigator(&selectedPart);
					}
					// box that holds the part's module tree
					CLAY(CLAY_ID("ModuleTreeBox"), { .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() } } })
					{
						// draw the modules
						DrawPartModules(selectedPart.modules, onlyRelevantModules);
					}
					// box that holds the list of the part's models
					CLAY(CLAY_ID("ModelListBox"), { .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() } } })
					{
						// draw the models
						DrawModels(selectedPart.models, selectedPart, onlyRelevantMaterials);
					}
					// box that holds the recolour configs
					CLAY(CLAY_ID("RecolourBox"), { .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() } } })
					{
						// draw the recolour config
						DrawRecolour(recolour);
					}
				}
			}
		);
		
		// collect all of the draw commands that are needed to draw the UI
		Clay_RenderCommandArray renderCommands = Clay_EndLayout(deltaTime);
		
		// enable raylib drawing
		BeginDrawing();
		ClearBackground(LIGHTGRAY);
		// get raylib to render the ui built by clay
		Clay_Raylib_Render(renderCommands, fonts);
		// disable raylib drawing
		EndDrawing();
		
		// free 
		FreeFreeQueue();
	}
	
	// De-Initialization
	//--------------------------------------------------------------------------------------
	CloseWindow();		// Close window and OpenGL context
	//--------------------------------------------------------------------------------------
	
	return 0;
}
