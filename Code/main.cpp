#include <windows.h>
#include <stdio.h>

#pragma pack(push, 1)
struct bit_map_header
{
	WORD   FileType;        /* File type, always 4D42h ("BM") */
	DWORD  FileSize;        /* Size of the file in bytes */
	WORD   Reserved1;       /* Always 0 */
	WORD   Reserved2;       /* Always 0 */
	DWORD  BitmapOffset;    /* Starting position of image data in bytes */

	DWORD  Size;            /* Size of this header in bytes */
	LONG   Width;           /* Image width in pixels */
	LONG   Height;          /* Image height in pixels */
	WORD   Planes;          /* Number of color planes */
	WORD   BitsPerPixel;    /* Number of bits per pixel */
    DWORD Compression;     /* Compression methods used */
	DWORD SizeOfBitmap;    /* Size of bitmap in bytes */
	LONG  HorzResolution;  /* Horizontal resolution in pixels per meter */
	LONG  VertResolution;  /* Vertical resolution in pixels per meter */
	DWORD ColorsUsed;      /* Number of colors in the image */
	DWORD ColorsImportant; /* Minimum number of important colors */
	
};
#pragma pack(pop)

struct bit_map
{
    void *Pixels;
    int Width;
    int Height;
    int BitsPerPixel;
    unsigned int *ColorPalette;
};

static void *
ReadEntireFile(char *FileName)
{
    HANDLE FileHandle =  CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle == INVALID_HANDLE_VALUE)
    {
        OutputDebugString("ERROR::OPENING::FILE\n");
        return 0;
    }
    LARGE_INTEGER FileSize;
    GetFileSizeEx(FileHandle, &FileSize);
    void *FileBuffer = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if(!ReadFile(FileHandle, FileBuffer, (DWORD)FileSize.QuadPart, 0, 0))
    {
        OutputDebugString("ERROR::READING::FILE\n");
        return 0;
    }
    return FileBuffer;
}

static bit_map
LoadBMP(char *FileName)
{
    bit_map Result = {};   
    void *BMPFile = ReadEntireFile(FileName);
    if(!BMPFile)
    {
        OutputDebugString("ERROR::LOADING::BMP\n");
        return Result;
    }

    bit_map_header *BMPHeader = (bit_map_header *)BMPFile;

    Result.Width = BMPHeader->Width;
    Result.Height = BMPHeader->Height;
    Result.BitsPerPixel = BMPHeader->BitsPerPixel;

    if(Result.BitsPerPixel == 8)
    {
        Result.ColorPalette = (unsigned int *)((unsigned char *)BMPFile + sizeof(bit_map_header));
        int StopHere = 0;
    }
    Result.Pixels = (void *)((unsigned char *)BMPFile + BMPHeader->BitmapOffset);

    return Result;
}

static void 
Bits8TransparentBlt(void *DestBuffer, BITMAPINFOHEADER *DestHeader,
               int XDest, int YDest, bit_map SourceBitmap)
{
    unsigned int *DestPtr = (unsigned int *)DestBuffer;
    unsigned char *SourcePtr = (unsigned char *)SourceBitmap.Pixels;

    int DestDeltaScan, DestWidthBytes, DestRealHeight;
    int SourceDeltaScan, XSource = 0, YSource = 0;
    int Width, Height;

    DestWidthBytes = DestHeader->biWidth; 
    // check if we have bottom-up or top-down BackBuffer setup
    if(DestHeader->biHeight < 0)
    {
        // Dest if Top-Down
        DestRealHeight = -DestHeader->biHeight; // get pisitive height
        DestDeltaScan = DestWidthBytes; // travel down dest
    }
    else
    {
        // Dest if Bottom-Up
        DestRealHeight = DestHeader->biHeight; 
        DestDeltaScan = -DestWidthBytes; // travel down dest
        // point to top scanline
        DestPtr += DestHeader->biWidth*DestHeader->biHeight - DestWidthBytes;
    }

    // Clip the source to dest
    Width = SourceBitmap.Width;
    Height = SourceBitmap.Height;
    if(XDest < 0)
    {
        // left clipped
        Width += XDest;
        XSource = -XDest;
        XDest = 0;
    }
    if((XDest + Width) > DestHeader->biWidth)
    {
        // right clipped
        Width = DestHeader->biWidth - XDest;
    }
    if(YDest < 0)
    {
        // top clipped
        Height += YDest;
        YSource = -YDest;
        YDest = 0;
    }
    if((YDest + Height) > DestRealHeight)
    {
        // bottom clipped
        Height = DestRealHeight - YDest;
    }
    
    SourceDeltaScan = SourceBitmap.Width;

    // step to starting source pixel
    SourcePtr += (SourceBitmap.Width*(SourceBitmap.Height - 1)) + XSource - (YSource*SourceDeltaScan);
    DestPtr += (YDest*DestDeltaScan)+XDest;

    // account for processed span in delta scans
    SourceDeltaScan += Width;
    DestDeltaScan -= Width;

    if((Height > 0) && (Width > 0))
    {
        // we have something to BLT
        for(int Y = 0;
            Y < Height;
            ++Y)
        {
            for(int X = 0;
                X < Width;
                ++X)
            {
                unsigned int Bits32SourceValue = SourceBitmap.ColorPalette[*SourcePtr];
                *DestPtr = Bits32SourceValue; // copy the pixel
                DestPtr++;
                SourcePtr++;
            }
            DestPtr += DestDeltaScan;
            SourcePtr -= SourceDeltaScan;
        }
    }

}

static void 
Bits32TransparentBlt(void *DestBuffer, BITMAPINFOHEADER *DestHeader,
               int XDest, int YDest, bit_map SourceBitmap)
{
    unsigned int *DestPtr = (unsigned int *)DestBuffer;
    unsigned int *SourcePtr = (unsigned int *)SourceBitmap.Pixels;
    int DestDeltaScan, DestWidthBytes, DestRealHeight;
    int SourceDeltaScan, XSource = 0, YSource = 0;
    int Width, Height;

    DestWidthBytes = DestHeader->biWidth; 
    // check if we have bottom-up or top-down BackBuffer setup
    if(DestHeader->biHeight < 0)
    {
        // Dest if Top-Down
        DestRealHeight = -DestHeader->biHeight; // get pisitive height
        DestDeltaScan = DestWidthBytes; // travel down dest
    }
    else
    {
        // Dest if Bottom-Up
        DestRealHeight = DestHeader->biHeight; 
        DestDeltaScan = -DestWidthBytes; // travel down dest
        // point to top scanline
        DestPtr += DestHeader->biWidth*DestHeader->biHeight - DestWidthBytes;
    }

    // Clip the source to dest
    Width = SourceBitmap.Width;
    Height = SourceBitmap.Height;
    if(XDest < 0)
    {
        // left clipped
        Width += XDest;
        XSource = -XDest;
        XDest = 0;
    }
    if((XDest + Width) > DestHeader->biWidth)
    {
        // right clipped
        Width = DestHeader->biWidth - XDest;
    }
    if(YDest < 0)
    {
        // top clipped
        Height += YDest;
        YSource = -YDest;
        YDest = 0;
    }
    if((YDest + Height) > DestRealHeight)
    {
        // bottom clipped
        Height = DestRealHeight - YDest;
    }
    
    SourceDeltaScan = SourceBitmap.Width;

    // step to starting source pixel
    SourcePtr += (SourceBitmap.Width*(SourceBitmap.Height - 1)) + XSource - (YSource*SourceDeltaScan);
    DestPtr += (YDest*DestDeltaScan)+XDest;

    // account for processed span in delta scans
    SourceDeltaScan += Width;
    DestDeltaScan -= Width;

    if((Height > 0) && (Width > 0))
    {
        // we have something to BLT
        for(int Y = 0;
            Y < Height;
            ++Y)
        {
            for(int X = 0;
                X < Width;
                ++X)
            {
                *DestPtr = *SourcePtr; // copy the pixel
                DestPtr++;
                SourcePtr++;
            }
            DestPtr += DestDeltaScan;
            SourcePtr -= SourceDeltaScan;
        }
    }

}

LRESULT CALLBACK WndProc(HWND   Window, UINT   Message, WPARAM WParam, LPARAM LParam);
void ProcesInputMessages(void);

static bool GlobalRunning;

int WINAPI WinMain(HINSTANCE Instance,
                   HINSTANCE PrevInstance,
                   LPSTR     lpCmdLine,
                   int       nShowCmd)
{
    WNDCLASSEX WindowClass = { 0 };
    WindowClass.cbSize = sizeof( WNDCLASSEX ) ;
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = WndProc;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
    WindowClass.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    WindowClass.lpszMenuName = NULL;
    WindowClass.lpszClassName = "Transparent BLTs";

    RegisterClassEx(&WindowClass);

    RECT Rect = { 0, 0, 640, 480 };
    AdjustWindowRect( &Rect, WS_OVERLAPPEDWINDOW, FALSE );

    HWND Window = CreateWindowA("Transparent BLTs",
                                "Transparent BLTs",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                Rect.right - Rect.left,
                                Rect.bottom - Rect.top,
                                NULL, NULL, Instance, NULL);
    if(Window)
    { 
        RECT ClientDimensions = {};
        GetClientRect(Window, &ClientDimensions);
        unsigned int Width  = ClientDimensions.right - ClientDimensions.left;
        unsigned int Height = ClientDimensions.bottom - ClientDimensions.top;
        int BackBufferSizeInBytes = Width*Height*sizeof(unsigned int);
        // Alloc memory for the back buffer.
        void *BackBuffer = VirtualAlloc(0, BackBufferSizeInBytes, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        
        BITMAPINFO BackBufferInfo = {};
        BackBufferInfo.bmiHeader.biSize = sizeof(BackBufferInfo.bmiHeader);
        BackBufferInfo.bmiHeader.biWidth = Width;
        BackBufferInfo.bmiHeader.biHeight = Height;
        BackBufferInfo.bmiHeader.biPlanes = 1;
        BackBufferInfo.bmiHeader.biBitCount = 32;
        BackBufferInfo.bmiHeader.biCompression = BI_RGB;

        HDC DeviceContext = GetDC(Window);
        HBITMAP BackBufferHandle = CreateDIBSection(DeviceContext, &BackBufferInfo,
                                                    DIB_RGB_COLORS, &BackBuffer, NULL, NULL);        

        bit_map Sabrina32BMP = LoadBMP("../Data/Sabrina.bmp");
        bit_map Sabrina8BMP = LoadBMP("../Data/Sabrina1.bmp");
        bit_map Camila32BMP = LoadBMP("../Data/Camila.bmp");
        bit_map Camila8BMP = LoadBMP("../Data/Camila1.BMP");

        unsigned int *Buffer = (unsigned int *)BackBuffer;
        for(int Y = 0;
            Y < Height;
            ++Y)
        {
            for(int X = 0;
                X < Width;
                ++X)
            {
                Buffer[Y*Width+X] = 0xFFFFCCCC; 
            }
        }
 
        Bits32TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                        0, 0, Sabrina32BMP);
        Bits8TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                       256, 0, Sabrina8BMP);
        Bits32TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                             0, 256, Camila32BMP);
        Bits8TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                       128, 256, Camila8BMP);

        GlobalRunning = true;
        ShowWindow(Window, nShowCmd);
        while(GlobalRunning)
        {
            ProcesInputMessages();

            HDC BackBufferDC = CreateCompatibleDC(DeviceContext);
            SelectObject(BackBufferDC, BackBufferHandle);
            BitBlt(DeviceContext, NULL, NULL, Width, Height, BackBufferDC, NULL, NULL, SRCCOPY);
            DeleteDC(BackBufferDC);
        }
         
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND   Window,
                         UINT   Message,
                         WPARAM WParam,
                         LPARAM LParam)
{
    LRESULT Result = {};
    switch(Message)
    {
        case WM_CLOSE:
        {
            GlobalRunning = false;
        }break;
        case WM_DESTROY:
        {
            GlobalRunning = false;
        }break;
        default:
        {
           Result = DefWindowProcA(Window, Message, WParam, LParam);
        }break; 
    }
    return Result;
}

void 
ProcesInputMessages(void)
{
    MSG Message = {};
    while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }
}


