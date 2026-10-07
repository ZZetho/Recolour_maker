// TODO list:
// add buttons to select actions
// add windows for each part of the program
// reorganise code
// when parsing modules, more checks for being commented out

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

const int nullTerminator = 1;
const int padding = 5;
const int fontSize = 20;
const Clay_Color textColour = (Clay_Color){ .r = 0, .b = 0, .g = 0, .a = 255};

//▀█▀ █▄█ █▀█ █▀▀ █▀
//░█░ ░█░ █▀▀ ██▄ ▄█

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

// the materials that are used on meshes
typedef struct ModelMaterial
{
	String materialName;
	String shaderName;
	StringArray meshes;		// the meshes that use this material
	KeyValueArray properties;
}
ModelMaterial;

// for the purposes of this program a model (.mu) is considered its materials and nothing else
typedef struct MuModel
{
	ModelMaterial * materials;
	int length;
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
	bool loaded;
}
PartInformation;


//█▀▄▀█ █ █▀ █▀▀
//█░▀░█ █ ▄█ █▄▄

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

// for printing ints
char * IntToString(int i)
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
	int nullByte = 1;
	int outputLength = significantFigures + negative + nullByte;
	char * output = malloc(sizeof(char) * outputLength);

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
		output[sigFigIndex + negative] = figure;
	}

	if (negative == true)
	{
		//printf("negative\n");
		output[0] = '-';
	}

	output[outputLength - 1] = '\0';
	//printf("%s\n", output);

	return output;
}

// converts a raylib colour to a clay colour
Clay_Color RaylibToClayColour(Color raylib)
{
	Clay_Color clay = (Clay_Color){ .r = raylib.r, .g = raylib.g, .b = raylib.b, .a = raylib.a };
	return clay;
}

//█▀ ▀█▀ █▀█ █ █▄░█ █▀▀   █▀▀ █░█ █▄░█ █▀▀ ▀█▀ █ █▀█ █▄░█ █▀
//▄█ ░█░ █▀▄ █ █░▀█ █▄█   █▀░ █▄█ █░▀█ █▄▄ ░█░ █ █▄█ █░▀█ ▄█

// finds the index of a char working backwards, or -1 if no char
int LastIndexOfChar(String string, int startingIndex, char target)
{
	if (startingIndex > string.length - 1 || startingIndex < 0)
	{
		printf("startingIndex: %d out of range: 0-%d\n", startingIndex, string.length - 1);
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
		printf("startingIndex: %d out of range: 0-%d\n", startingIndex, string.length - 1);
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

// adds a charchter to the end of a string
String AddCharToEndOfString(String string, char newChar)
{
//	printf("old string: ");
//	BetterPrint(string);

	string.length += 1;
	string.chars = realloc(string.chars, sizeof(char) * string.length);
	string.chars[string.length - 1] = newChar;
	
//	printf("new string: ");
//	BetterPrint(string);
	
	return string;
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
	*string = AddCharToEndOfString(*string, '\0');
	
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
Clay_String MyToClayString(String myString)
{
	// create a palce to store the chars as we cant memcpy to Clay_String.chars as it is const
	char * chars = malloc(sizeof(char) * myString.length);
	memcpy(chars, myString.chars, sizeof(char) * myString.length);
	// point to the new char array
	Clay_String string = (Clay_String){ .isStaticallyAllocated = false, .length = myString.length, .chars = chars };
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
		memcpy(string->chars, &string->chars[1], sizeof(char) * string->length);
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

// check if two strings are equal
bool StringsEqual(String a, String b)
{
	if (a.chars == NULL)
	{
		printf("string a has NULL chars\n");
		return false;
	}
	if (b.chars == NULL)
	{
		printf("string b has NULL chars\n");
		return false;
	}
	if (a.length != b.length)
	{
		return false;
	}
	
	for (int charIndex = 0; charIndex < a.length && charIndex < b.length; charIndex ++)
	{
		if (a.chars[charIndex] != b.chars[charIndex])
		{
			return false;
		}
	}
	
	return true;
}

// extracts a string from a larger string
String ExtractString(String extractee, int startIndex, int endIndex)
{
	if (startIndex < 0)
	{
		//printf("ERROR: start index: %d was less than zero\n", startIndex);
		return (String){ 0 };
	}
	
	if (endIndex <= startIndex)
	{
		//printf("ERROR: end index: %d was less than or equal to start index: %d\n", endIndex, startIndex);
		return (String){ 0 };
	}
	
	if (startIndex > extractee.length - 1)
	{
		//printf("ERROR: start index: %d was after extractee end\n", startIndex);
		return (String){ 0 };
	}
	
	if (endIndex > extractee.length - 1)
	{
		//printf("ERROR: end index: %d was after extractee end\n", endIndex);
		return (String){ 0 };
	}
	
	//	   5	 11
	//	 V	 V
	//extractee.string
	
	// create a new string to put the extracted chars into
	String extracted = { 0 };
	int extractedLength = endIndex - startIndex + 1;
	ResizeString(&extracted, extractedLength);
	
	// copy into the extracted string from the extractee
	memcpy(extracted.chars, &extractee.chars[startIndex], sizeof(char) * extracted.length);
	return extracted;
}

// adds the second string to the end of the first one
String ConcatenateStrings(String stringOne, String stringTwo)
{
	GarunteeNullTerminator(&stringOne);
	GarunteeNullTerminator(&stringTwo);
	
//	BetterPrint(stringOne);
//	BetterPrint(stringTwo);
//	printf("being combined\n");
	
	// egg\0			(length 4)
	// cheese\0			(length 7)
	// -> eggcheese\0	(length 10)
	
	String output = { 0 };
	int stringTwoAddIndex = stringOne.length - 1;
	int outputLength = stringOne.length + stringTwo.length - 1;
	ResizeString(&output, outputLength);
	
	memcpy(output.chars, stringOne.chars, sizeof(char) * stringOne.length);
	memcpy(&output.chars[stringTwoAddIndex], stringTwo.chars, sizeof(char) * stringTwo.length);
	
	return output;
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

// returns the index of a key in a key value array
int IndexOfKey(KeyValueArray array, String key)
{
	//printf("keys and values: %d\n", array.length);
	for (int index = 0; index < array.length; index ++)
	{
		if (StringsEqual(array.keysValues[index].key, key) == true)
		{
			return index;
		}
	}
	
	return -1;
}

// finds the value associated with a key
String ValueOfKey(KeyValueArray array, String key)
{
	int keyIndex = IndexOfKey(array, key);
	
	if (keyIndex == -1)
	{
		return (String){ .chars = NULL, .length = 0 };
	}
	
	String value = array.keysValues[keyIndex].value;
	return value;
}

//█▀▀ █ █░░ █▀▀   █░█ ▄▀█ █▄░█ █▀▄ █░░ █ █▄░█ █▀▀
//█▀░ █ █▄▄ ██▄   █▀█ █▀█ █░▀█ █▄▀ █▄▄ █ █░▀█ █▄█

// resets current directory to the KSP install folder
void ResetCurrentDirectory(String * directory)
{
	// should start at the folder called "Kerbal Space Program"
	
	int initialDirectorylength = 7; 			// string plus null byte
	const char * initialDirectory = "../../";	
	
	*directory = (String){ .length = initialDirectorylength, .chars = realloc(directory->chars, sizeof(char) * initialDirectorylength)};
	memcpy(directory->chars, initialDirectory, sizeof(char) * initialDirectorylength);
	
	return;
}

// reads a whole file as a string
String ReadFile(String filePath)
{
	// try and open the file
	FILE * file = fopen(filePath.chars, "r");
	
	if (file == NULL)
	{
		int err = errno;
		printf("ERROR: Could not open: %s because: %s\n", filePath.chars, strerror(err));
		
		return (String){ 0 };
	}
	
	printf("reading file: %s\n", filePath.chars);
	
	// read the data from the config file
	const size_t bufferSize = 100;
	String buffer = (String){ 0 };
	ResizeString(&buffer, bufferSize);
	
	int writeIndex = 0;					// the index to write to in the otuput from the buffer
	String output = (String) { 0 };
	ResizeString(&output, 0);			// clear the otuput
	
	size_t readObjectCount = 0;	// how many chars have been read this read cycle
	
	while (true)
	{
		// read the next 100 chars
		readObjectCount = fread(buffer.chars, sizeof(char), buffer.length, file);
		
		// add the number read chars
		int configFileSize = output.length + readObjectCount;
		ResizeString(&output, configFileSize);
		
		// copy the read chars from the buffer to the config file string
		memcpy(&output.chars[writeIndex], buffer.chars, sizeof(char) * readObjectCount);
		
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
	
	GarunteeNullTerminator(&output);
	
	// the whole config file has been read, close it
	fclose(file);
	
	return output;
}

// read from an array of file paths
StringArray ReadFiles(StringArray filePaths)
{
	StringArray filesContents = (StringArray){ .strings = NULL, .length = 0 };
	
	for (int index = 0; index < filePaths.length; index ++)
	{
		String fileContents = ReadFile(filePaths.strings[index]);
		
		if (fileContents.length == 0)
		{
			// if failed to read from the file
			continue;
		}
		
		filesContents.length += 1;
		filesContents.strings = realloc(filesContents.strings, sizeof(String) * filesContents.length);
		filesContents.strings[filesContents.length - 1] = fileContents;
	}
	
	return filesContents;
}

//█▀▀ █▀█ █▄░█ █▀▀ █ █▀▀   █▀▀ █ █░░ █▀▀   █▀█ ▄▀█ █▀█ █▀ █ █▄░█ █▀▀
//█▄▄ █▄█ █░▀█ █▀░ █ █▄█   █▀░ █ █▄▄ ██▄   █▀▀ █▀█ █▀▄ ▄█ █ █░▀█ █▄█

// check if a config file is a .cfg for a part
bool IsPartConfig(String configFile)
{
	// look for "PART" in the top-level node
	int nodeDepth = 0;
	
	for (int readIndex = 0; readIndex + 5 < configFile.length; readIndex ++)
	{
		//printf("chars at:\n%d: %c\n%d: %c\n%d: %c\n%d: %c\n%d: %c\n%d: %c\n", readIndex, configFile.chars[readIndex], readIndex + 1, configFile.chars[readIndex + 1], readIndex + 2, configFile.chars[readIndex + 2], readIndex + 3, configFile.chars[readIndex + 3], readIndex + 4, configFile.chars[readIndex + 4], readIndex + 5, configFile.chars[readIndex + 5]);
		if (configFile.chars[readIndex] == '{')
		{
			nodeDepth += 1;
		}
		else if (configFile.chars[readIndex] == '}')
		{
			nodeDepth -= 1;
		}
		
		// look for PART\n{
		// or PART\r\n{
		if (
			nodeDepth == 0 
			&& configFile.chars[readIndex] == 'P'
			&& configFile.chars[readIndex + 1] == 'A'
			&& configFile.chars[readIndex + 2] == 'R'
			&& configFile.chars[readIndex + 3] == 'T'
			&& 
			((
				configFile.chars[readIndex + 4] == '\n'
				&& configFile.chars[readIndex + 5] == '{'
			) || (
				configFile.chars[readIndex + 4] == '\r'
				&& configFile.chars[readIndex + 5] == '\n'
				&& configFile.chars[readIndex + 6] == '{'
			)))
		{
			printf("is a part config file, found PART\\n{ at: %d\n", readIndex);
			return true;
		}
	}
	
	printf("not a part config file\n");
	return false;
}

// returns the index of the next //
int IndexOfNextComment(String configFile, int startIndex)
{
	for (int index = startIndex; index < configFile.length - 1; index ++)
	{
		if (configFile.chars[index] == '/' && configFile.chars[index + 1] == '/')
		{
			return index;
		}
		//printf("looking for comment at: %d\n", index);
	}
	
	// no // found
	return -1;
}

void ParseModuleProperties(String configFile, int startIndex, int endIndex, Module * module)
{
	// between the start and end, parse the properties and values of each line
	// first off, find the next pair of \n's, everything in between is a line
	// should be in the format: property = value
	// look for the =
	// the property will be everything between the = and start of the line (\n)
	// value is everything between the = and the \n or //
	// value may include whitespace and single /'s
	// property probably cant have whitespace? whatevs
	// trim whitespace from the ends of property and value
	
	// once all properties have been parsed, look for the 'name' propety
	// and set it as this modules name
	
	int lineStart;
	int lineEnd = startIndex;
	
	while (lineEnd < endIndex)
	{
		// the start and end of the line
		lineStart = lineEnd;	// end of the last line is the start of the next one
		lineEnd = IndexOfChar(configFile, lineStart + 1, '\n');
		
		//printf("new line end: %d\n", lineEnd);
		//printf("config file length: %d\n", configFile.length);
		
		//printf("line end index: %d\n", lineEnd);
		
		if (lineStart == -1 || lineEnd == -1)
		{
			//hit the end of the file
			return;
		}
		
		// look for the =
		int equalsIndex = IndexOfChar(configFile, lineStart, '=');
		int commentIndex = IndexOfNextComment(configFile, equalsIndex);
		
		if (commentIndex == -1)
		{
			// no comment found
			commentIndex = INT32_MAX;
		}
		
		if (equalsIndex == -1 || equalsIndex > lineEnd || commentIndex < equalsIndex)
		{
			// no = found on this line
			// or line is commented out
			continue;
		}
		
		//printf("equals index: %d\n", equalsIndex);
		//printf("line start: %d\n", lineStart);
		
		// everything from \n to =
		String preEquals = { .length = equalsIndex - lineStart - 1, .chars = NULL };
		preEquals.chars = realloc(preEquals.chars, sizeof(char) * preEquals.length);
		memcpy(preEquals.chars, &configFile.chars[lineStart + 1], sizeof(char) * preEquals.length);
		// 0		  12
		// V		   V
		//\n....cheese.=.egg...\n
		
		//printf("preEquals chars: %d\n", preEquals.length);
		
		
		// find where the value ends
		int postEqualsEndIndex;
		if (commentIndex < lineEnd)
		{
			postEqualsEndIndex = commentIndex;
		}
		else
		{
			postEqualsEndIndex = lineEnd;
		}
		
		//everything from = to \n or //
		String postEquals = { .length = postEqualsEndIndex - equalsIndex - 1, .chars = NULL };
		postEquals.chars = realloc(postEquals.chars, sizeof(char) * postEquals.length);
		memcpy(postEquals.chars, &configFile.chars[equalsIndex + 1], sizeof(char) * postEquals.length);
		//			  12	   20
		//			 V	   V
		//\n....cheese.=.egg...\n
		
		GarunteeNullTerminator(&preEquals);
		GarunteeNullTerminator(&postEquals);
		
		TrimWhitespace(&preEquals);
		TrimWhitespace(&postEquals);
		
		//BetterPrint(preEquals);
		//printf("=\n");
		//BetterPrint(postEquals);
		
		printf("%s = %s\n", preEquals.chars, postEquals.chars);
		//printf("line end: %d\n", lineEnd);
		
		// convert the strings to a key value pair and add them to the module
		module->properties.length += 1;
		module->properties.keysValues = realloc(module->properties.keysValues, sizeof(KeyValue) * module->properties.length);
		
		module->properties.keysValues[module->properties.length - 1].key = (String){ .length = preEquals.length, .chars = malloc(sizeof(char) * preEquals.length)};
		module->properties.keysValues[module->properties.length - 1].value = (String){ .length = postEquals.length, .chars = malloc(sizeof(char) * postEquals.length)};
		
		memcpy(module->properties.keysValues[module->properties.length - 1].key.chars, preEquals.chars, sizeof(char) * preEquals.length);
		memcpy(module->properties.keysValues[module->properties.length - 1].value.chars, postEquals.chars, sizeof(char) * postEquals.length);
	}
	
	// see if any of the keys are "name"
	// set the modules name if so
	String name = (String){ .length = 5, .chars = "name" };
	int nameIndex = IndexOfKey(module->properties, name);
	
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
	String type = ExtractString(configFile, lineStart + 1, bracketIndex - 1);
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
	int previousLineComment = IndexOfNextComment(configFile, previousLineStart);
	
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
	type = ExtractString(configFile, previousLineStart + 1, previousLineEnd - 1);
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

// represents the modules that part.cfg files are made of
//typedef struct Module
//{
//	struct Module * children;
//	int childCount;
//	struct Module * parent;
//	String type;
//	String * name;				// should point to the value of the property with the key 'name'
//	KeyValueArray properties;
//}
//Module;

void BlankModule(Module * module)
{
	module->children = (ModuleArray){ .modules = NULL, .length = 0 };
	module->parent = NULL;
	module->type = (String){ .length = 0, .chars = NULL };
	module->name = NULL;
	module->properties = (KeyValueArray){ .length = 0, .keysValues = NULL };
}

//typedef struct PartInformation
//{
//	String configFilePath;
//	String configFileContents;
//	String modelDataPath;
//	String modelDataContents;
//	Module modules;
//	bool loaded;
//}

// changes all of the properties of a part to 0 or NULL
void BlankPart(PartInformation * part)
{
	part->configFilePath.length = 0;
	part->configFilePath.chars = realloc(part->configFilePath.chars, 0);
	part->configFileContents.length = 0;
	part->configFileContents.chars = realloc(part->configFileContents.chars, 0);
	BlankModule(&part->modules);
	part->loaded = false;
}

// loads a .cfg file into part information if it is a config file for a part
// returns true / false if it succedded or not
bool LoadConfigFile(PartInformation * part, String configFilePath)
{
	// open the .cfg (.txt) file at currentDirectory/fileName and turn it into a string
	// only if it is the config file for a part
	// combine the filename and current directory to get the full file path
	
	// try and read the config file
	String configFileContents = ReadFile(configFilePath);
	
	// if the config is not for a part, throw it out
	if (IsPartConfig(configFileContents) == false)
	{
		ResizeString(&part->configFileContents, 0);
		return false;
	}
	
	// if the file is a part config file, clear the part object, ready for new data to be added to it
	BlankPart(part);
	part->configFilePath = configFilePath;
	part->configFileContents = configFileContents;
	
	GarunteeNullTerminator(&part->configFilePath);
	GarunteeNullTerminator(&part->configFileContents);
	
	printf("opened config file: %s\n", part->configFilePath.chars);
	printf("contents:\n%s\n", part->configFileContents.chars);
	
	// index of the first {
	// this is where modules start being defined
	int modulesStartIndex = IndexOfChar(part->configFileContents, 0, '{');
	
	// parse the config string into a tree of modules
	ParseModules(part->configFileContents, modulesStartIndex, &part->modules);
	part->loaded = true;
	
	return true;
}

//█▀▄▀█ █▀█ █▀▄ █▀▀ █░░ █▀▄ ▄▀█ ▀█▀ ▄▀█   █▀█ ▄▀█ █▀█ █▀ █ █▄░█ █▀▀
//█░▀░█ █▄█ █▄▀ ██▄ █▄▄ █▄▀ █▀█ ░█░ █▀█   █▀▀ █▀█ █▀▄ ▄█ █ █░▀█ █▄█

// uses a path to a config file to find the path to the parts model data file
String ModelPathToModelDataPath(String modelPath)
{
	// convert the file path to the model to a file path to model data
	// from: 	ModName/modelName
	// to:		../../exportedUVMaps/ModName/modelName/ModelData.txt
	
	// initialise the model data path as the path to the model model
	String modelDataPath = { 0 };
	ResizeString(&modelDataPath, modelPath.length);
	memcpy(modelDataPath.chars, modelPath.chars, sizeof(char) * modelPath.length);
	
	// add "../../exportedUVMaps/" to the start
	int exportedUVMapsLength = 21;
	ResizeString(&modelDataPath, modelDataPath.length + exportedUVMapsLength);
	// move the string over to allow adding exportedUVMaps
	memcpy(&modelDataPath.chars[exportedUVMapsLength], modelDataPath.chars, sizeof(char) * modelPath.length);
	// add exportedUVMaps
	memcpy(modelDataPath.chars, "../../exportedUVMaps/", sizeof(char) * exportedUVMapsLength);
	
	// add "/ModelData.txt" to the end
	int modelDataLength = 14;
	int modelDataPathLength = modelDataPath.length;
	ResizeString(&modelDataPath, modelDataPath.length + modelDataLength);
	memcpy(&modelDataPath.chars[modelDataPathLength - 1], "/ModelData.txt", sizeof(char) * modelDataLength);
	
	printf("converted config file path to: ");
	BetterPrint(modelDataPath);
	//printf("length: %d\n", modelDataPath.length);
	
	return modelDataPath;
}

// converts an array of model paths to modelData.txt paths
StringArray ModelPathsToModelDataPaths(StringArray modelPaths)
{
	StringArray modelDataPaths = (StringArray){ .strings = NULL, .length = 0 };
	
	for (int index = 0; index < modelPaths.length; index ++)
	{
		String modelDataPath = ModelPathToModelDataPath(modelPaths.strings[index]);
		modelDataPaths.length += 1;
		modelDataPaths.strings = realloc(modelDataPaths.strings, sizeof(String) * modelDataPaths.length);
		modelDataPaths.strings[modelDataPaths.length - 1] = modelDataPath;
	}
	
	return modelDataPaths;
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
	
	//printf("mesh name: ");
	//BetterPrint(meshName);
	
	return meshName;
}

// checks if the model already has a material with that name, returns its index, or -1 if none were found
int HasMaterialWithName(MuModel model, String materialName)
{
	// if the model has no materials it cannot have a material called materialName
	if (model.length <= 0)
	{
		return -1;
	}
	
	// check the name against the names of the materials that the model has
	for (int index = 0; index < model.length; index ++)
	{
		if (StringsEqual(model.materials[index].materialName, materialName) == true)
		{
			return index;
		}
	}
	
	return -1;
}

// returns the property and its value of a line in a modelData file
KeyValue ParseMeshProperty(String propertyLine)
{
	//			  V (12)
	//..property..:..value.. (22)
	int colonIndex = IndexOfChar(propertyLine, 0, ':');
	
	if (colonIndex == -1)
	{
		printf("no : found\n");
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
//       Shader Name               : KSP/Bumped Specular
//       RenderQueue               : 2000
//       _MainTex                  : NearFutureSpacecraft/Parts/FuelTank/service-tank/servicetank-25 (UnityEngine.Texture2D)
//       _MainTexOffset            : (0.0, 0.0)
//       _BumpMap                  : NearFutureSpacecraft/Parts/FuelTank/service-tank/servicetank-25-n (UnityEngine.Texture2D)
//       _BumpMapOffset            : (0.0, 0.0)
//       _Shininess                : 0.3
//       _Color                    : (1.0, 1.0, 1.0, 1.0)
//
//++++++++++++++++++
//-------------------------------------------

// makes a material with all properties 0 or NULL
void EmptyMaterial(ModelMaterial * material)
{
	*material = (ModelMaterial){ .materialName = (String){ .chars = NULL, .length = 0 }, .shaderName = (String){ .chars = NULL, .length = 0 }, .meshes = (StringArray){ .strings = NULL, .length = 0 }, .properties = (KeyValueArray){ .keysValues = NULL, .length = 0 }};
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
void AddMesh(String mesh, ModelMaterial * material)
{
	material->meshes.length += 1;
	material->meshes.strings = realloc(material->meshes.strings, sizeof(String) * material->meshes.length);
	material->meshes.strings[material->meshes.length - 1] = mesh;
}

// reads the information about the material that a mesh uses
void MaterialPropertiesFromMesh(ModelMaterial * material, String modelData, String meshName, int materialStart)
{	
	// first line is minus row
	// second line is mesh name
	// third line is render type
	// fourth line is plus row
	// fifth line is material #
	// sixth line is material name
	// seventh is shader name
	// eighth and onwards are properties and values
	
	printf("reading mesh: ");
	BetterPrint(meshName);

	int firstNewLine = IndexOfChar(modelData, materialStart + 1, '\n');
	int secondNewLine = IndexOfChar(modelData, firstNewLine + 1, '\n');
	int thirdNewLine = IndexOfChar(modelData, secondNewLine + 1, '\n');
	int fourthNewLine = IndexOfChar(modelData, thirdNewLine + 1, '\n');
	int fifthNewLine = IndexOfChar(modelData, fourthNewLine + 1, '\n');
	int sixthNewLine = IndexOfChar(modelData, fifthNewLine + 1, '\n');
	int seventhNewLine = IndexOfChar(modelData, sixthNewLine + 1, '\n');
	
	String thirdLine = ExtractString(modelData, secondNewLine + 1, thirdNewLine - 1);
	String fifthLine = ExtractString(modelData, fourthNewLine + 1, fifthNewLine - 1);
	String sixthLine = ExtractString(modelData, fifthNewLine + 1, sixthNewLine - 1);
	String seventhLine = ExtractString(modelData, sixthNewLine + 1, seventhNewLine - 1);
	
//	printf("second line: ");
//	BetterPrint(secondLine);
//	printf("third line: ");
//	BetterPrint(thirdLine);
//	printf("fifth line: ");
//	BetterPrint(fifthLine);
//	printf("sixth line: ");
//	BetterPrint(sixthLine);
//	printf("seventh line: ");
//	BetterPrint(seventhLine);
	
	EmptyMaterial(material);
	
	AddMesh(meshName, material);
	AddProperty(ParseMeshProperty(thirdLine), material);
	AddProperty(ParseMeshProperty(fifthLine), material);
	material->materialName = ParseMeshProperty(sixthLine).value;
	material->shaderName = ParseMeshProperty(seventhLine).value;
	
//	BetterPrint(material->materialName);
//	BetterPrint(material->shaderName);
	
	int lineStart;
	int lineEnd = seventhNewLine;
	
	while (true)
	{
		lineStart = lineEnd;
		lineEnd = IndexOfChar(modelData, lineStart + 1, '\n');
		
		String line = ExtractString(modelData, lineStart + 1, lineEnd - 1);
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
	}
	
	return;
}

void ParseModelData(MuModel * model, String modelData)
{
	// modelData.txt is made up of a series of meshes, which are bordered, on either side, by a row of -'s: -------------------------------------------
	// within that is the mesh name, render type and a material which is bordered, on either side, by a row of +'s: (++++++++++++++++++)
	
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
		
		// find the name of the material the mesh uses
		String materialName = FindMaterialName(modelData, meshStartIndex);
		// find the name of the mesh
		String meshName = FindMeshName(modelData, meshStartIndex);
		
		// check if this material has alredy been found
		int materialIndex = HasMaterialWithName(*model, materialName);
		
		// if this material has already been seen, add the name of this mesh to the found material
		if (materialIndex != -1)
		{
			// add this mesh to the list of meshes of the material that this mesh uses
			ModelMaterial * existingMaterial = &model->materials[materialIndex];
			AddMesh(meshName, existingMaterial);
			continue;
		}
		
		// create a new material
		model->length += 1;
		model->materials = realloc(model->materials, sizeof(ModelMaterial) * model->length);
		ModelMaterial * newMaterial = &model->materials[model->length - 1];
		
		// get the properties of the new material from the mesh
		MaterialPropertiesFromMesh(newMaterial, modelData, meshName, meshStartIndex);
		
		printf("found material: ");
		BetterPrint(newMaterial->materialName);
	}
	
	printf("found: %d materials\n", model->length);
	
	free(minusRow.chars);
	free(plusRow.chars);
}

// parse an array of model data files
void ParseModelDatas(ModelArray * models, StringArray modelDatas)
{
	// clear out the models
	*models = (ModelArray){ .models = NULL, .length = 0 };
	
	for (int index = 0; index < modelDatas.length; index ++)
	{
		String modelData = modelDatas.strings[index];
		
		// create a new model to parse modelData into
		models->length += 1;
		models->models = realloc(models->models, sizeof(Model) * models->length);
		MuModel * model = &models->models[models->length - 1];
		*model = (MuModel){ .materials = NULL, .length = 0 };
		
		ParseModelData(model, modelData);
	}
	
	printf("%d models\n", models->length);
}

// returns every child module that has a specific name
//ModuleArray SearchForModuleName(Module * module, String name)
//{
//	for (int index = 0; index < module->children.length)
//}
//
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

// returns the file path after the "model =" in each of the models
StringArray GetModelPaths(ModuleArray models)
{
	StringArray modelPaths = (StringArray){ .strings = NULL, .length = 0 };
	
	for (int index = 0; index < models.length; index ++)
	{
		Module model = models.modules[index];
		String modelPath = ValueOfKey(model.properties, (String){ .chars = "model", .length = 6 });
		
		if (modelPath.length == 0)
		{
			printf("failed to find key: \"module\"\n");
			continue;
		}
		
		modelPaths.length += 1;
		modelPaths.strings = realloc(modelPaths.strings, sizeof(String) * modelPaths.length);
		modelPaths.strings[modelPaths.length - 1] = modelPath;
	}
	
	return modelPaths;
}

// load the modelData.txt files associated with the part
// there will be one for each model the part uses
void LoadModelData(PartInformation * part)
{
	// find all the "MODEL" modules a part
	ModuleArray modelModules = (ModuleArray){ .modules = NULL, .length = 0 };
	SearchForModuleType(&part->modules, (String){ .chars = "MODEL", .length = 6 }, &modelModules);
	printf("found: %d models\n", modelModules.length);
	
	// find the model paths of the models
	StringArray modelPaths = GetModelPaths(modelModules);
	
	// covert the model path to the path to their modelData.txt
	StringArray modelDataPaths = ModelPathsToModelDataPaths(modelPaths);
	
	// read each of the model data files
	StringArray modelDataFilesContents = ReadFiles(modelDataPaths);
	
	// check that each of the modelData.txt's were read
	for (int index = 0; index < modelDataFilesContents.length; index ++)
	{
		String modelDataFileContents = modelDataFilesContents.strings[index];
		
		// if failed to read modelData.txt
		if (modelDataFileContents.length == 0)
		{
			MuModel * model = &part->models.models[index];
			// make the model have a single material that indicates failure to load ModelData.txt
			model->length = 1;
			model->materials = realloc(model->materials, sizeof(ModelMaterial) * model->length);
			
			ModelMaterial * errorMaterial = &model->materials[0];
			EmptyMaterial(errorMaterial);
			errorMaterial->materialName = (String){ .chars = "ERROR: failed to read ModelData.txt", .length = 36 };
		}
		printf("Model data file contents:\n%s\n", modelDataFileContents.chars);
	}
	
	// parse the model data file into an array of materials
	ParseModelDatas(&part->models, modelDataFilesContents);
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
	int fileIndex = fileName.length - 1;
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
			String filePath = ConcatenateStrings(*selectedDirectory, clickedDirectory);
			
			// try to load the config file as a string
			if (LoadConfigFile(part, filePath) == true)
			{
				// if that succeded, try and load ModelData.txt
				LoadModelData(part);
			}
			
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

// draw the enteries of the selected directory and manage what happens if a directory is clicked
void DirectoryNavigator(String * selectedDirectoryPath, PartInformation * part)
{
	// settings
	Clay_Color directoryBackground = RaylibToClayColour(WHITE);
	Clay_Color directoryHovered = RaylibToClayColour(SKYBLUE);
	
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
	
	// create a scrolling box to display the directories entries in
	CLAY(CLAY_ID("DirectoryNavigator"), { .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding, .layoutDirection = CLAY_TOP_TO_BOTTOM}, .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() } })
	{
		// draw all the entries in the directory
		while (entry != NULL)
		{
			// draw a button with the name of the directory entry
			CLAY_AUTO_ID({ .cornerRadius = 5, .backgroundColor = Clay_Hovered() ? directoryHovered : directoryBackground, .layout = { .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding) } })
			{
				// copy the directories name out of the dirent and add a null terminator
				String directoryName = (String){ .length = strlen(entry->d_name), .chars = malloc(strlen(entry->d_name)) };
				memcpy(directoryName.chars, entry->d_name, sizeof(char) * directoryName.length);
				GarunteeNullTerminator(&directoryName);
				// draw the name of the directory
				CLAY_TEXT(MyToClayString(directoryName), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
				
				// if this directory has been clicked on navigate to this directory
				if (Clay_Hovered() && IsMouseButtonPressed(0))
				{
					NavigateDirectory(selectedDirectoryPath, directoryName, part);
				}
			}
			// read the next entry in the directory
			entry = readdir(selectedDirectory);
		}
		
		closedir(selectedDirectory);
	}
}

//█▀▄ █▀█ ▄▀█ █░█░█ █ █▄░█ █▀▀
//█▄▀ █▀▄ █▀█ ▀▄▀▄▀ █ █░▀█ █▄█

void DrawArrow(Vector2 startPos, Vector2 endPos, int width, Color colour)
{
	Vector2 arrowHeadVector = Vector2Scale(Vector2Subtract(startPos, endPos), 0.2);
	Vector2 arrowHeadOrthogonal = {.x = arrowHeadVector.y, .y = -arrowHeadVector.x};
		
	Vector2 arrowHeadLeftVector = Vector2Add(arrowHeadVector, arrowHeadOrthogonal);
	Vector2 arrowHeadRightVector = Vector2Subtract(arrowHeadVector, arrowHeadOrthogonal);
	DrawLineEx(startPos, endPos, width, colour);
	DrawLineEx(endPos, Vector2Add(arrowHeadLeftVector, endPos), width, colour);
	DrawLineEx(endPos, Vector2Add(arrowHeadRightVector, endPos), width, colour);
}

// returns the height of the drawn properties
void DrawModule(Module module)
{
	// draw the modules type
	Clay_String type = MyToClayString(module.type);
	CLAY_TEXT(type, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
	
	// draw the modules properties
	for (int propertyIndex = 0; propertyIndex < module.properties.length; propertyIndex ++)
	{
		// the property to draw
		KeyValue property = module.properties.keysValues[propertyIndex];
		
		CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() } } })
		{
			// draw the key on the left
			Clay_String key = MyToClayString(property.key);
			CLAY_TEXT(key, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			
			CLAY_AUTO_ID({ .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
			{
				// draw the value on the right
				Clay_String value = MyToClayString(property.value);
				CLAY_TEXT(value, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			}
		}
	}
	
	// draw an arrow connecting to the verticall line drawn by the parent
	// TODO: redo this
//	int halfHeight = moduleHeight / 2;
//	int halfChildHorizontalOffset = 25;
//	DrawArrow((Vector2){ .x = moduleXPosition - halfChildHorizontalOffset, .y = moduleYPosition + halfHeight }, (Vector2){ .x = moduleXPosition, .y = moduleYPosition + halfHeight }, 1, RED);
}

// draws the module tree, depth first
void DrawModuleTree(Module module)
{
	// draw the module,
	// create a box below the module to put children in
	// draw each child in a stack a bit to the right in that box
	
	CLAY_AUTO_ID({ .layout = { .padding = CLAY_PADDING_ALL(padding) } })
	{
		CLAY_AUTO_ID({ .cornerRadius = 5, .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .padding = CLAY_PADDING_ALL(padding) } })
		{
			// draw this module
			DrawModule(module);
		}
	}
	
	// create the boxes for children
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		// filler box to move the childen over a bit
		CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(50), .height = CLAY_SIZING_GROW() } } }) {}
		// box to put the children in
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM } })
		{
			for (int index = 0; index < module.children.length; index ++)
			{
				// draw the childen
				DrawModuleTree(module.children.modules[index]);
			}
		}
	}
}

// draws a single material
void DrawMaterial(ModelMaterial material)
{
	// draw the materials name
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		CLAY_TEXT(CLAY_STRING("Material name: "), { .textColor = textColour, .fontSize = fontSize, .fontId = 0});
		
		CLAY_AUTO_ID({ .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			Clay_String materialName = MyToClayString(material.materialName);
			CLAY_TEXT(materialName, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
		}
	}
	
	// draw the shader type
	CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		CLAY_TEXT(CLAY_STRING("Shader type: "), { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
		
		CLAY_AUTO_ID({ .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
		{
			Clay_String shaderName = MyToClayString(material.shaderName);
			CLAY_TEXT(shaderName, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
		}
	}
	
	// draw the meshes
	CLAY_TEXT(CLAY_STRING("Meshes that use this material:"), { .textColor = textColour, .fontSize = fontSize, .fontId = 0});
	CLAY_AUTO_ID({ .layout = { .padding = { .left = 2 * padding, .right = 0, .top = 0, .bottom = 0 }, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
	{
		for (int index = 0; index < material.meshes.length; index ++)
		{
			Clay_String mesh = MyToClayString(material.meshes.strings[index]);
			CLAY_TEXT(mesh, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
		}
	}
	
	// draw the properties
	for (int index = 0; index < material.properties.length; index ++)
	{
		KeyValue property = material.properties.keysValues[index];
		
		CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .height = CLAY_SIZING_FIT(), .width = CLAY_SIZING_GROW() } } })
		{
			// draw the key on the left
			Clay_String key = MyToClayString(property.key);
			CLAY_TEXT(key, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			
			// draw the value on the right
			CLAY_AUTO_ID({ .layout = { .childAlignment = CLAY_ALIGN_X_RIGHT, .sizing = { .height = CLAY_SIZING_GROW(), .width = CLAY_SIZING_GROW() } } })
			{
				Clay_String value = MyToClayString(property.value);
				CLAY_TEXT(value, { .textColor = textColour, .fontSize = fontSize, .fontId = 0 });
			}
		}
	}
}

// draws the materials of a model
void DrawMaterials(MuModel model)
{
	CLAY_AUTO_ID({ .cornerRadius = 5, .backgroundColor = RaylibToClayColour(WHITE), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding } })
	{
		//printf("drawing: %d materials\n", model.length);
		for (int index = 0; index < model.length; index ++)
		{
			ModelMaterial material = model.materials[index];
			DrawMaterial(material);
		}
	}
}

// draws an array of models
void DrawModels(ModelArray models)
{
	CLAY(CLAY_ID("Models"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() }, .padding = CLAY_PADDING_ALL(padding), .childGap = padding }, .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() } })
	{
		//printf("drawing: %d models\n", models.length);
		for (int index = 0; index < models.length; index ++)
		{
			MuModel model = models.models[index];
			DrawMaterials(model);
		}
	}
}

//█▀▄▀█ ▄▀█ █ █▄░█
//█░▀░█ █▀█ █ █░▀█

int main(void)
{
	//-------------------------------------------------------------------------------------- Initialization
	int screenWidth = 1080;
	int screenHeight = 800;
	const char * title = "Recolour maker";
	
	Clay_Raylib_Initialize(screenWidth, screenHeight, title, FLAG_WINDOW_RESIZABLE);
	
	Font fonts[2];
	fonts[0] = LoadFontEx("Roboto-Regular.ttf", 48, 0, 400);
	fonts[1] = GetFontDefault();
	
	uint64_t clayRequiredMemory = Clay_MinMemorySize();
	Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayRequiredMemory, malloc(clayRequiredMemory));
	Clay_Initialize(clayMemory, (Clay_Dimensions){ .width = GetScreenWidth(), .height = GetScreenHeight() }, (Clay_ErrorHandler){ .errorHandlerFunction = HandleClayErrors, .userData = NULL });
	Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);
	
	Clay_SetDebugModeEnabled(true);
	
	// the path to the currently selected directory in the file browser
	String selectedDirectoryPath = { 0 };
	ResetCurrentDirectory(&selectedDirectoryPath);
	
	// the currently selected part
	PartInformation selectedPart = { 0 };
	
	SetTargetFPS(60); // target framerate
	SetExitKey(KEY_BACKSPACE); // key to close the game
	
	//-------------------------------------------------------------------------------------- Update Loop
	while (!WindowShouldClose()) // my loope ❤️😁️
	{
		float deltaTime = GetFrameTime();
		Vector2 mousePosition = GetMousePosition();
		Vector2 mouseWheelMovement = GetMouseWheelMoveV();
		
		// hold shift to scroll faster
		float scrollMultiplier = 10;
		if (IsKeyDown(KEY_LEFT_SHIFT) == true || IsKeyDown(KEY_RIGHT_SHIFT) == true)
		{
			mouseWheelMovement = Vector2Scale(mouseWheelMovement, scrollMultiplier);
		}
		
		// clay update logic
		Clay_SetLayoutDimensions((Clay_Dimensions){ .width = GetScreenWidth(), .height = GetScreenHeight() });
		Clay_SetPointerState((Clay_Vector2){ .x = mousePosition.x, .y = mousePosition.y }, IsMouseButtonDown(0));
		Clay_UpdateScrollContainers(true, (Clay_Vector2){ .x = mouseWheelMovement.x, .y = mouseWheelMovement.y }, deltaTime);
		
		// enable ui building
		Clay_BeginLayout();
		
		// ui settings
		int navBarHeight = 100;
		
		// a window that fills the whole screen
		CLAY(CLAY_ID("MainWindow"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })
		{
			// navbar
			CLAY(CLAY_ID("NavBar"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(navBarHeight) } }, .backgroundColor = RaylibToClayColour(WHITE) })
			{
				
			}
			// holds the boxes that hold the functions of the program
			CLAY(CLAY_ID("Content"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } } })
			{
				// the box that holds the directory navigator
				CLAY(CLAY_ID("NavigatorBox"), { .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(0.2f), .height = CLAY_SIZING_GROW() } } })
				{
					// draw the directory navigator
					DirectoryNavigator(&selectedDirectoryPath, &selectedPart);
				}
				// box that holds the part's module tree
				CLAY(CLAY_ID("ModuleTreeBox"), { .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(0.4f), .height = CLAY_SIZING_GROW() } } })
				{
					CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() } }, .clip = { .vertical = true, .horizontal = true, .childOffset = Clay_GetScrollOffset() } })
					{
						// draw the module tree
						DrawModuleTree(selectedPart.modules);
					}
				}
				// box that holds the part's models
				CLAY(CLAY_ID("ModelsBox"), { .layout = { .sizing = { .width = CLAY_SIZING_PERCENT(0.4f), .height = CLAY_SIZING_GROW() } } })
				{
					// draw the models
					DrawModels(selectedPart.models);
				}
			}
		}
		
		// collect all of the draw commands that are needed to draw the UI
		Clay_RenderCommandArray renderCommands = Clay_EndLayout(deltaTime);
		
		// enable raylib drawing
		BeginDrawing();
		ClearBackground(LIGHTGRAY);
		// get raylib to render the ui built by clay
		Clay_Raylib_Render(renderCommands, fonts);
		// disable raylib drawing
		EndDrawing();
	}
	
	// De-Initialization
	//--------------------------------------------------------------------------------------
	CloseWindow();		// Close window and OpenGL context
	//--------------------------------------------------------------------------------------
	
	return 0;
}
