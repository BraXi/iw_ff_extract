/*
ffdump_extract
Extract rawfiles from any CoD game after dumping FF with offzip.
All gsc, csc, gsx, atr, txt, csv, vision, shock, cfg and vehicle defs

Written years ago by BraXi
https://github.com/BraXi/iw_ff_extract

Licensed under GNU General Public License v2.

To bo honest, this code is very primitive and naive and was written because I was curious about CoD4 alphas, but *it just works*. Really, don't judge me.
I was able to extract all rawfiles from all games that I've tested including CoD2 (Xbox), CoD4 alphas for Xbox, CoD4 PC, CoDWaW PC and Xbox alpha.
This could likely work with other games such as MW2, BO, MW3 and others, but I have not tested.


USAGE:
First, you need to deflate .FF file with offzip utility which will find and extract all zlib blocks it can find in fastfile.
Next, use ffdump_extract to find and extract all rawfiles from that dump file: ffdump_extract c:/path/to/dumpfile.dat c:/outfiles
Confusingly, it will spam you with a lot of errors about failed file writes, you need to make directories yourself before dumping! ;-)
Finally, enjoy if possible, hah.


HOW TF IT WORKS:
1. load dumpfile
    2. iterate thru all bytes in dump:
        2a. find extension of rawfile ".<ext><NULL>" 
        2b. ...or vehicledef "<255><255><255><255>vehicle/{name}<NULL>"
        2c. set rawfiletype flag
    3. iterate backwards to find full filename untill <NULL> or <255> but if its a vehicledef then iterate forward untill <NULL>
    4. save filename and mark startpos at {filenameEND}+1 (startpos is first byte after null terminated filename)
    5. now iterate from startpos untill <NULL> to find endpos of raw file
6. save raw files to disk
*/

// TODO: extract weapons too
// TODO: FF in alpha versions of cod4 have localizedstrings sorted nicely: "<LOCALIZED STRINGS FILE NAME>_<LOCALIZED STRING>" {..trash..} "{STRING}<NULL>"


#define _CRT_SECURE_NO_WARNINGS 1

#include <iostream>
using namespace std;


// Diferent RAWFILE types
#define RAWFILE_NONE 0
#define RAWFILE_SCRIPT 1
#define RAWFILE_ANIMTREE 2
#define RAWFILE_TXT 3
#define RAWFILE_CSV 4
#define RAWFILE_VISION 5
#define RAWFILE_SHOCK 6
#define RAWFILE_CFG 7
#define RAWFILE_VEHICLE 8

// ... and their human readable forms
const char rawTypeStr[9][16] =
{   "UNKNOWN",
    "script",
    "animtree",
    "txt",
    "csv",
    "vision",
    "shock",
    "config",
    "vehicle"
};

// marks the beginning of vehiclefile
long    vehicleFileStartPos = 0;


// Rawfiles during process are stored in arrays below
#define MAX_RAW_FILES 512 // this is completly out of my mind, can be any higher value tbh or a dynamic array
#define MAX_RAW_FILE_NAME 128 // and this too

long    rawStartPos[MAX_RAW_FILES];
long    rawEndPos[MAX_RAW_FILES];
long    rawSize[MAX_RAW_FILES];
int     rawType[MAX_RAW_FILES];
char    rawFileName[MAX_RAW_FILES][MAX_RAW_FILE_NAME];

unsigned int    numRAWFiles = 0; // number of rawfiles found in dump data


char* Config_OutDir = NULL;

// These two are filled with LoadDumpFile
static char* rawData = NULL;
static long rawDataSize = 0;



// ==================================
// utilities
bool isNan(char c)
{
    if (c == (char)255)
        return true;
    return false;
}

bool IsPositionSafe(long pos)
{
    if (pos >= rawDataSize || pos < 0)
        return false;
    return true;
}

char getb(long pos)
{
    if (!IsPositionSafe(pos) /*|| !rawData*/)
    {
        cout << "error: getb(" << pos << ") out of bounds" << endl;
        exit(1);
    }
    return rawData[pos];
}

bool isASCII(char ch)
{
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
        return true;
    return false;
}
// ==================================



/*
* bool ReadDumpFile(const char* fileName)
*
* Loads dump file into memory
*/
bool ReadDumpFile(const char* fileName)
{
    FILE* f = NULL;

	printf("loading dump: %s...", fileName);
    if (!(f = fopen(fileName, "rb")))
    {
		printf(" failed, no such file.\n");
        return false;
    }

    fseek(f, 0, SEEK_END);
    rawDataSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (!rawDataSize)
    {
		printf("%s failed, file is empty.\n", fileName);
        exit(1);
    }

    rawData = new char[rawDataSize + 1];
    if (!rawData)
    {
        fclose(f);
		printf("failed to allocate rawData\n");
        exit(1);
    }

    fread(rawData, rawDataSize, 1, f);
    fclose(f);

	printf("done, %d bytes total.\n", rawDataSize);
    rawData[rawDataSize] = NULL; //null terminated
    return true;
}


/*
* void FindRawFileContents(long startPos)
*
* Traverses from startPos forward untill it finds NULL to mark start and end positions as well as size of rawfile contents
*/
void FindRawFileContents(long startPos)
{
    long pos = 0;
    char b = NULL;

    rawStartPos[numRAWFiles] = startPos;
    pos = startPos;
    while ((b = getb(pos)) != (char)NULL)
    {
 //       if (rawType[numRAWFiles] == RAWFILE_VEHICLE)
//            cout << b;
        rawSize[numRAWFiles]++;
        pos++;

        if (!IsPositionSafe(pos))
        {
			printf("Error: FindRawFileContents() unexpected end of data\n");
            exit(1);
        }
    }
    rawEndPos[numRAWFiles] = pos;
//    if (rawType[numRAWFiles] == RAWFILE_VEHICLE)
 //   cout << endl;
}


/*
* void FindRawFileName(long endPos)
*
* Goes back in data from endPos (usually from pos before hitting NULL) untill it hits NULL or '255' to find rawfile name
*/
void FindRawFileName(long endPos)
{
    long pos = 0;
    char b = NULL;

    long startPos = endPos;
    pos = endPos;
    b = getb(pos);

    while (b != (char)NULL && !isNan(b))
    {
        pos--;
        b = getb(pos);
        startPos = pos;
    }

    pos = startPos+1;
    b = getb(pos);
    int namelen = 0;
    memset(rawFileName[numRAWFiles], 0, sizeof(*rawFileName[numRAWFiles]));
    while (b != (char)NULL)
    {
        rawFileName[numRAWFiles][namelen] = b;
        namelen++;
//        cout << b;
        pos++;
        b = getb(pos);
    }
    rawFileName[numRAWFiles][namelen+1] = NULL;
//    cout << endl;
}


/*
* bool FindVehicleFile(long pos)
*
* Does search for magic bytes from a given start position in data to find vehicledata
* Returns true on success and marks vehicleFileStartPos where vehicletype data starts
*/
bool FindVehicleFile(long pos)
{
    if (!IsPositionSafe(pos + 12))
        return false; // when no 12 bytes forward are safe we can just skip at all

    // 4 255s before "vehicles/"
    if (!isNan(rawData[pos]) && !isNan(rawData[pos + 1]) && !isNan(rawData[pos + 2]) && !isNan(rawData[pos + 3]))
        return false;

    // first find "vehicles/" str
    if (rawData[pos + 4] == 'v' && rawData[pos + 5] == 'e' && rawData[pos + 6] == 'h' && rawData[pos + 7] == 'i' && rawData[pos + 8] == 'c' && rawData[pos + 9] == 'l' && rawData[pos + 10] == 'e' && rawData[pos + 11] == 's' && rawData[pos + 12] == '/' )
    {
        long i = pos + 4;
        char c = 0;
        while ((c = rawData[i]) != (char)NULL && i < rawDataSize) // what we do here is we read data untill NULL to find "vehicles/somevehiclename"
        {
            i++;
        }         
        vehicleFileStartPos = i+1; // After vehicle name is NULL, and after that is the first byte of vehiclefile
        return true;
    }

    return false;
}


/*
* unsigned int FindRawFile(long pos)
* 
* Does search for magic bytes from a given start position in data to find asset in dumpfile
* Returns RAWFILE_TYPE>0 on success
*/
unsigned int FindRawFile(long pos)
{
    if (!IsPositionSafe(pos+4))
        return RAWFILE_NONE;

    if (rawData[pos] == '.' && rawData[pos + 4] == (char)NULL)
    {
        //GSC script ".gsc"
        if (rawData[pos + 1] == 'g' && rawData[pos + 2] == 's' && rawData[pos + 3] == 'c')
        {
            return RAWFILE_SCRIPT;
        }

        //CODWAW: CSC script ".csc"
        if (rawData[pos + 1] == 'c' && rawData[pos + 2] == 's' && rawData[pos + 3] == 'c')
        {
            return RAWFILE_SCRIPT;
        }

        //COD4X: GSX script ".gsx"
        if (rawData[pos + 1] == 'g' && rawData[pos + 2] == 's' && rawData[pos + 3] == 'x')
        {
            return RAWFILE_SCRIPT;
        }

        //Animtree ".atr"
        if (rawData[pos + 1] == 'a' && rawData[pos + 2] == 't' && rawData[pos + 3] == 'r')
        {
            return RAWFILE_ANIMTREE;
        }

        //Text ".txt"
        if (rawData[pos + 1] == 't' && rawData[pos + 2] == 'x' && rawData[pos + 3] == 't')
        {
            return RAWFILE_TXT;
        }

        //CSV ".csv"
        if (rawData[pos + 1] == 'c' && rawData[pos + 2] == 's' && rawData[pos + 3] == 'v')
        {
            return RAWFILE_CSV;
        }

        //Config ".cfg"
        if (rawData[pos + 1] == 'c' && rawData[pos + 2] == 'f' && rawData[pos + 3] == 'g')
        {
            return RAWFILE_CFG;
        }
    }

    //Shock ".shock"
    if (IsPositionSafe(pos + 6) && rawData[pos] == '.' && rawData[pos + 1] == 's' && rawData[pos + 2] == 'h' && rawData[pos + 3] == 'o' && rawData[pos + 4] == 'c' && rawData[pos + 5] == 'k' && rawData[pos + 6] == (char)NULL)
    {
        return RAWFILE_SHOCK;
    }

    //Vision ".vision"
    if (IsPositionSafe(pos + 7) && rawData[pos] == '.' && rawData[pos + 1] == 'v' && rawData[pos + 2] == 'i' && rawData[pos + 3] == 's' && rawData[pos + 4] == 'i' && rawData[pos + 5] == 'o' && rawData[pos + 6] == 'n' && rawData[pos + 7] == (char)NULL)
    {
        return RAWFILE_VISION;
    }

    //Vehicle "vehicles/{...}"
    if( FindVehicleFile(pos) )
    {
        return RAWFILE_VEHICLE;
    }

    return RAWFILE_NONE;
}



/*
* void ProcessDumpFile()
*
* Goes through all the data in dumpfile in order to find worthy bytes in this pile of garbage ;v
*/
bool ProcessDumpFile()
{
    long pos = 0;
    int type = 0;

	printf("processing file, this may take a while...\n\n");
    for (pos = 0; pos != rawDataSize; pos++)
    {
        type = FindRawFile(pos);
        if (type == RAWFILE_NONE)
            continue;

//        cout << "found # " << numRAWFiles << " - \"" << rawTypeStr[type] << "\" at " << pos << endl;
        rawType[numRAWFiles] = type;

        if (type == RAWFILE_VEHICLE)
        {
            FindRawFileName(pos + 12);
            FindRawFileContents(vehicleFileStartPos);
        }
        else if (type == RAWFILE_VISION)
        {
            FindRawFileName(pos + 6);
            FindRawFileContents(pos + 8);
        }
        else if (type == RAWFILE_SHOCK)
        {
            FindRawFileName(pos + 5);
            FindRawFileContents(pos + 7);
        }
        else
        {
            FindRawFileName(pos + 3);
            FindRawFileContents(pos + 5);
        }
        numRAWFiles++;

        if (numRAWFiles >= MAX_RAW_FILES)
        {
			printf("FATAL ERROR! Increase MAX_RAW_FILES and recompile\n");
            exit(1);
        }
    }

    if (numRAWFiles)
    {
		printf("Found %i raw files.\n", numRAWFiles);
        return true;
    }

	printf("NO RAWFILES FOUND!\n", numRAWFiles);
    return false;
}

#if 0
bool safe_mkdir(const char* dirname)
{
    int result = _mkdir(dirname);
    if (result != 0)
        return errno == EEXIST;
    return true;
}

bool CreateDirectoryTree(const char* path)
{
}
#endif

void WriteRawFiles()
{
    FILE* fptr = NULL;
    char* outdata = NULL;
    char outname[512];

    if (!numRAWFiles)
        return;

	printf("\nWriting rawfiles to: %s\n", Config_OutDir);
    for (int i = 0; i != numRAWFiles; i++)
    {
        memset(outname, 0, sizeof(*outname));
        sprintf(outname, "%s/%s", Config_OutDir, rawFileName[i]);

        fptr = fopen(outname, "w");

        if (fptr == NULL)
        {
            printf("failed to write %s\n", outname);
            exit(1);
        }

        outdata = new char[rawSize[i]];
        if (!outdata)
        {
            exit(1);
        }
        memcpy(outdata, &rawData[rawStartPos[i]], rawSize[i]);
        fwrite(outdata, rawSize[i], 1, fptr);
        fclose(fptr);
        delete[]outdata;
    }
	printf("done.\n");

}

int main(int argc, char* argv[])
{
    printf("idiotproof fast file extractor for IWEngine games\n\n");
    if (argc < 3)
    {
		printf("First off, extract fast file with offzip, then use this utility to extract worthy data\n");
		printf("usage: ffdump_extract.exe dumpfile outdir\n");
		printf("example: ffdump_extract.exe C:/cod4/common.ff.dump C:/out\n");
        system("pause");
        return 1;
    }

    Config_OutDir = argv[2];

    if (!ReadDumpFile(argv[1]))
        return 1;

    cout << endl;

    if (!ProcessDumpFile())
    {
        if (rawData != NULL)
            delete[]rawData;
        exit(1);
    }

    for (int i = 0; i != numRAWFiles; i++)
		printf("#%i %s -> %s\n", i, rawTypeStr[rawType[i]], rawFileName[i]);
//        cout << "#" << i << ": " << rawTypeStr[rawType[i]] << " -> " << rawSize[i] << " bytes, pos (" << rawStartPos[i] << "-" << rawEndPos[i] << ") - " << rawFileName[i] << endl;
    WriteRawFiles();

    if( rawData != NULL )
        delete[]rawData;

    return 0;
}
