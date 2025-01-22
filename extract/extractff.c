
// GNU GPLv2 License.

/*
 ALL file names are NULL terminated
 PC/XBOX have diferent endianess

 String Tables:
 ==============
 [int32 num_columns] [int32 num_rows] [FF FF FF FF] [ASCII filename] 00 [some junk] [some form of _sometimes_ compressed? data]  00
 must figure out the junk part
 
 
 Shaders/Techniques
 ==================
 all cod4/waw builds have shader model 2 & 3

 00 00 10 14 - "shader" type?
 00 00 10 0D - vertex shader SM2
 00 00 00 0D - pixel shader SM2

 00 00 10 14 00 00 10 0D [shader_name] 2A [technique_name] 00 - SM2 vertex shader?
 00 00 10 14 00 00 00 0D [shader_name] 2A [technique_name] 00 - SM2 pixel shader?
 00 00 10 14 00 00 10 16 [shader_name] 2A [technique_name] 00 - SM3 pixel shader?
 00 00 10 14 00 00 10 17 - same?
*/
#define TOOL_VERSION ("0.2 (" __DATE__ ")")

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_RAW_FILES 512 // seems to be enough for all cases
#define MAX_RAW_FILE_NAME 128

// RAWFILE types
typedef enum RawFileType
{
    RAWFILE_NONE = 0,
    RAWFILE_SCRIPT,     // gsc, gsx, csc
    RAWFILE_ANIMTREE,   // atr
    RAWFILE_TXT,        // various text files
    RAWFILE_CSV,        // string tables
    RAWFILE_VISION,     // vision configs
    RAWFILE_SHOCK,      // shock files
    RAWFILE_CFG,        // configs
    RAWFILE_DEF,        // sound channel definitions
    RAWFILE_ARENA,      // map lists
    RAWFILE_VEHICLE,    // vehicle defs
    RAWFILE_MENU,
    RAWFILE_ANIMSCRIPT,
    RAWFILE_INFO,

    NUM_RAWFILE_TYPES
} RawFileType_t;

const char *RawFileTypeName[NUM_RAWFILE_TYPES] =
{ 
    "UNKNOWN",
    "script",
    "animtree",
    "txt",
    "csv",
    "vision",
    "shock",
    "config",
    "def",
    "arena",
    "vehicle",
    "menu",
    "animscr",
    "info"
};

// marks the beginning of info file, currently: lochit table, penetration table and vehicledef
long    infoFileStartPos = 0;

// information about rawfiles found in data is keept in here
typedef struct rawfile_s
{
    RawFileType_t type;
    char    name[MAX_RAW_FILE_NAME];
    long    start_pos; // if start_pos & end_pos == 0 then write dummy file
    long    end_pos;
    long    size;
} rawfile_t;

rawfile_t rawFiles[MAX_RAW_FILES];
rawfile_t *p_rawfile; // ptr to the current rawfile
unsigned int numFoundFiles = 0; // number of files found

char* Config_OutDir = NULL;

// These two are filled with LoadDumpFile
static char* rawData = NULL;
static long rawDataSize = 0;

int isNan(char c)
{
    if (c == (char)255)
        return 1;
    return 0;
}

int IsPositionSafe(long pos)
{
    if (pos >= rawDataSize || pos < 0)
        return 0;
    return 1;
}

char getb(long pos)
{
    if (!IsPositionSafe(pos) /*|| !rawData*/)
    {
        printf("Error: getb(%i) out of bounds\n", pos);
        exit(1);
    }
    return rawData[pos];
}

int isASCII(char ch)
{
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'))
        return 1;
    return 0;
}
// ==================================



// Loads dump file into memory
int ReadDumpFile(const char* fileName)
{
    FILE* f;

    printf("Loading '%s'... ", fileName);

    f = fopen(fileName, "rb");
    if (!f)
    {
        printf("failed, no such file.\n");
        return 0;
    }

    // find the size
    fseek(f, 0, SEEK_END);
    rawDataSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (!rawDataSize)
    {
        printf("failed, file is empty.\n");
        exit(1);
    }

    rawData = malloc(sizeof(char) * (rawDataSize + 1));
    if (!rawData)
    {
        fclose(f);
        printf("failed to malloc.\n");
        exit(1);
    }

    fread(rawData, rawDataSize, 1, f);
    fclose(f);

    printf("done, %d bytes total.\n", rawDataSize);
    rawData[rawDataSize] = 0; //null terminated
    return 1;
}



// Traverses from startPos forward untill it finds NULL to mark start and end positions as well as size of rawfile contents
void FindRawFileContents(long startPos)
{
    long pos = 0;
    char b = 0;

    p_rawfile->start_pos = startPos;
    pos = startPos;
    while( (b = getb(pos)) != 0 )
    {
        p_rawfile->size++;
        pos++;

        if (!IsPositionSafe(pos))
        {
            printf("Error: FindRawFileContents() unexpected end of data\n");
            exit(1);
        }
    }
    p_rawfile->end_pos = pos;
}



// Goes back in data from endPos (usually from pos before hitting NULL) untill it hits NULL or '255' to find rawfile name
void FindRawFileName(long endPos)
{
    long pos = 0;
    char b = 0;

    long startPos = endPos;
    pos = endPos;
    b = getb(pos);

    while (b != 0 && !isNan(b))
    {
        pos--;
        b = getb(pos);
        startPos = pos;
    }

    pos = startPos + 1;
    b = getb(pos);
    int namelen = 0;

    memset(p_rawfile->name, 0, sizeof(p_rawfile->name));
    while (b != (char)NULL)
    {
        p_rawfile->name[namelen] = b;
        namelen++;
        pos++;
        b = getb(pos);
    }

    p_rawfile->name[namelen + 1] = 0; // NULL terminate
}



// Does search for magic bytes from a given start position in data to find vehicledata
// Returns true on success and marks vehicleFileStartPos where vehicletype data starts
int FindVehicleFile(long pos)
{
    if (!IsPositionSafe(pos + 12))
        return 0; // when no 12 bytes forward are safe we can just skip at all

    // find FF FF FF FF
    if (!isNan(rawData[pos]) && !isNan(rawData[pos + 1]) && !isNan(rawData[pos + 2]) && !isNan(rawData[pos + 3]))
        return 0;

    // first find "vehicles/" str
    if (rawData[pos + 4] == 'v' && rawData[pos + 5] == 'e' && rawData[pos + 6] == 'h' && rawData[pos + 7] == 'i' && rawData[pos + 8] == 'c' && rawData[pos + 9] == 'l' && rawData[pos + 10] == 'e' && rawData[pos + 11] == 's' && rawData[pos + 12] == '/')
    {
        long i = pos + 4;
        char c = 0;
        while ((c = rawData[i]) != (char)NULL && i < rawDataSize) // what we do here is we read bytes untill NULL to find "vehicles/somevehiclename"
        {
            i++;
        }
        infoFileStartPos = i + 1; // vehicledef name is followed by a NULL, and after that is the first byte of info
        return 1;
    }

    return 0;
}


// Does search for magic bytes from a given start position in data to find info table
// Returns true on success and marks infoFileStartPos where info data starts
int FindInfoFile(long pos)
{
    if (!IsPositionSafe(pos + 8))
        return 0; // when no 8 bytes forward are safe we can just skip at all

    // find FF FF FF FF
    if (!isNan(rawData[pos]) && !isNan(rawData[pos + 1]) && !isNan(rawData[pos + 2]) && !isNan(rawData[pos + 3]))
        return 0;

    // first find "info/" str
    if (rawData[pos + 4] == 'i' && rawData[pos + 5] == 'n' && rawData[pos + 6] == 'f' && rawData[pos + 7] == 'o' && rawData[pos + 8] == '/')
    {
        long i = pos + 4;
        char c = 0;
        while ((c = rawData[i]) != (char)NULL && i < rawDataSize) // what we do here is we read bytes untill NULL to find "info/someinfoname"
        {
            i++;
        }
        infoFileStartPos = i + 1; // info name is followed by a NULL, and after that is the first byte of info
        return 1;
    }

    return 0;
}

// Does search for magic bytes from a given start position in data 
// to find asset in dumpfile. Returns > RAWFILE_NONE on success.
RawFileType_t FindRawFile(long pos)
{
    if (!IsPositionSafe(pos + 4))
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

        // COD4X: GSX script ".gsx"
        if (rawData[pos + 1] == 'g' && rawData[pos + 2] == 's' && rawData[pos + 3] == 'x')
        {
            return RAWFILE_SCRIPT;
        }

        // Animtree ".atr"
        if (rawData[pos + 1] == 'a' && rawData[pos + 2] == 't' && rawData[pos + 3] == 'r')
        {
            return RAWFILE_ANIMTREE;
        }

        // Text ".txt"
        if (rawData[pos + 1] == 't' && rawData[pos + 2] == 'x' && rawData[pos + 3] == 't')
        {
            return RAWFILE_TXT;
        }

        // CSV ".csv" (doesn't read them correctly yet)
        if (rawData[pos + 1] == 'c' && rawData[pos + 2] == 's' && rawData[pos + 3] == 'v')
        {
            return RAWFILE_CSV;
        }

        // Config ".cfg"
        if (rawData[pos + 1] == 'c' && rawData[pos + 2] == 'f' && rawData[pos + 3] == 'g')
        {
            return RAWFILE_CFG;
        }

        // Sound channel definition file ".def"
        if (rawData[pos + 1] == 'd' && rawData[pos + 2] == 'e' && rawData[pos + 3] == 'f')
        {
            return RAWFILE_DEF;
        }

    }

    // Map lists ".arena"
    if (IsPositionSafe(pos + 6) && rawData[pos] == '.' && rawData[pos + 1] == 'a' && rawData[pos + 2] == 'r' && rawData[pos + 3] == 'e' && rawData[pos + 4] == 'n' && rawData[pos + 5] == 'a' && rawData[pos + 6] == (char)NULL)
    {
        return RAWFILE_ARENA;
    }

    // UI ".menu" (a dummy file will be made)
    if (IsPositionSafe(pos + 5) && rawData[pos] == '.' && rawData[pos + 1] == 'm' && rawData[pos + 2] == 'e' && rawData[pos + 3] == 'n' && rawData[pos + 4] == 'u' && rawData[pos + 5] == (char)NULL)
    {
        return RAWFILE_MENU;
    }

    // Shock ".shock"
    if (IsPositionSafe(pos + 6) && rawData[pos] == '.' && rawData[pos + 1] == 's' && rawData[pos + 2] == 'h' && rawData[pos + 3] == 'o' && rawData[pos + 4] == 'c' && rawData[pos + 5] == 'k' && rawData[pos + 6] == (char)NULL)
    {
        return RAWFILE_SHOCK;
    }

    // Player animation script ".script"
    if (IsPositionSafe(pos + 7) && rawData[pos] == '.' && rawData[pos + 1] == 's' && rawData[pos + 2] == 'c' && rawData[pos + 3] == 'r' && rawData[pos + 4] == 'i' && rawData[pos + 5] == 'p' && rawData[pos + 6] == 't' && rawData[pos + 7] == (char)NULL)
    {
        return RAWFILE_ANIMSCRIPT;
    }

    // Vision settings ".vision"
    if (IsPositionSafe(pos + 7) && rawData[pos] == '.' && rawData[pos + 1] == 'v' && rawData[pos + 2] == 'i' && rawData[pos + 3] == 's' && rawData[pos + 4] == 'i' && rawData[pos + 5] == 'o' && rawData[pos + 6] == 'n' && rawData[pos + 7] == (char)NULL)
    {
        return RAWFILE_VISION;
    }

    // Vehicle definitiion "vehicles/{...}"
    if (FindVehicleFile(pos))
    {
        return RAWFILE_VEHICLE;
    }

    // Info table "info/{...}"
    if (FindInfoFile(pos))
    {
        return RAWFILE_INFO;
    }

    return RAWFILE_NONE;
}

// Goes through all the data in dumpfile in order to find worthy bytes in this pile of garbage ;v
int ProcessDumpFile()
{
    long pos;
    RawFileType_t type = RAWFILE_NONE;

    printf("Processing file, this may take a while...\n");
    for (pos = 0; pos != rawDataSize; pos++)
    {
        type = FindRawFile(pos);
        if (type == RAWFILE_NONE)
            continue;

        //printf("Found file #%i of type %s at %i\n", numRAWFiles, rawTypeString[type], pos);
        p_rawfile->type = type;

        if (type == RAWFILE_VEHICLE)
        {
            FindRawFileName(pos + 12);
            FindRawFileContents(infoFileStartPos);
        }
        else if (type == RAWFILE_INFO)
        {
            FindRawFileName(pos + 8);
            FindRawFileContents(infoFileStartPos);
        }
        else if (type == RAWFILE_ANIMSCRIPT)
        {
            FindRawFileName(pos + 6);
            FindRawFileContents(pos + 8);
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
        else if (type == RAWFILE_ARENA)
        {
            FindRawFileName(pos + 5);
            FindRawFileContents(pos + 7);
        }
        else if (type == RAWFILE_MENU)
        {
            FindRawFileName(pos + 4);
            // TODO: decompile menus
            p_rawfile->start_pos = p_rawfile->end_pos = 0; // don't bother writing junk
        }     
        else
        {
            FindRawFileName(pos + 3);
            FindRawFileContents(pos + 5);
        }

        p_rawfile++;
        numFoundFiles++;

        if (numFoundFiles >= MAX_RAW_FILES)
        {
            printf("Error: Increase MAX_RAW_FILES.\n");
            exit(1);
        }
    }

    if (numFoundFiles)
    {
        return 1;
    }

    printf("--- No files to extract were found. ---\n");
    return 0;
}

void CreatePath(char* path)
{
    char* ofs;
    for (ofs = path + 1; *ofs; ofs++)
    {
        if (*ofs == '/')
        {
            *ofs = 0;
            _mkdir(path);
            *ofs = '/';
        }
    }
}

void WriteRawFiles()
{
    FILE* fptr = NULL;
    char* outdata = NULL;
    char outname[512];
    char dummy_str[512];

    if (!numFoundFiles)
        return;

    printf("\nWriting %i rawfiles to: %s\n", numFoundFiles, Config_OutDir);
    for (int i = 0; i != numFoundFiles; i++)
    {
        memset(outname, 0, sizeof(*outname));
        sprintf(outname, "%s/%s", Config_OutDir, rawFiles[i].name);

        CreatePath(outname);
        fptr = fopen(outname, "wb");

        if (fptr == NULL)
        {
            printf("Failed to open '%s' for writing.\n", outname);
            exit(1);
        }

        if (rawFiles[i].start_pos == 0 && rawFiles[i].end_pos == 0)
        {
            printf("Writing dummy file '%s' (extractor is too stupid to decompile menus)\n", rawFiles[i].name);
            sprintf(dummy_str, "// File '%s' cannot be decompiled with this version of extractor, its dummied out.\0", rawFiles[i].name);
            fwrite(dummy_str, strlen(dummy_str), 1, fptr);
            fclose(fptr);
            continue;
        }

        outdata = malloc(sizeof(char) * rawFiles[i].size);
        if (!outdata)
        {
            exit(1);
        }
       
        memcpy(outdata, &rawData[rawFiles[i].start_pos], rawFiles[i].size);
        fwrite(outdata, rawFiles[i].size, 1, fptr);
        fclose(fptr);
        free (outdata);
    }
    printf("done.\n");

}

int main(int argc, char* argv[])
{
    unsigned int i;

    printf("Simple FastFile Extractor for IWEngine games %s\ngithub.com/braxi/iw_ff_extract\n\n", TOOL_VERSION);

    if (argc < 3)
    {
        printf("Usage: extractff.exe zlib_dump.dat outdir\n");
        printf("First off, extract ZLIB blocks from FastFile with 'offzip' program, then use this utility to extract worthy data.\n");
           
        printf("\nExample Usage:\n");
        printf("Step1: offzip.exe -a code_post_gfx.ff : Extract & deflate ZLIB blocks from FF and write them to file.\n");
        printf("Step2: extractff.exe 0000001c.dat c:/code_post_gfx : Writes all rawfiles to 'code_post_gfx' dir.\n");
        system("pause");
        return 1;
    }

    memset(&rawFiles, 0, sizeof(rawFiles));
    p_rawfile = &rawFiles[0];

    Config_OutDir = argv[2];

    if (!ReadDumpFile(argv[1]))
        return 1;

    if (!ProcessDumpFile())
    {
        if (rawData != NULL)
            free(rawData);

        exit(1);
    }

    printf("\nFound %i files:\n", numFoundFiles);
    for (i = 0; i < numFoundFiles; i++)
    {
        printf("%3i %9s : '%s'\n", i + 1, RawFileTypeName[rawFiles[i].type], rawFiles[i].name);
    }

      WriteRawFiles();

    if (rawData != NULL)
        free(rawData);

    return 0;
}