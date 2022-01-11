#include <thermal.h>
#include "FreeRTOS.h"
#include "framebuffer.h"
#include <cstring>
#include <math.h>

#include "task.h"

IRSensor::IRSensor()
{
	this->dma2dHandler = NULL;
	this->fb_addr = 0;
	this->minTemp = 0;
	this->maxTemp = 0;
	this->layer = 0;
	this->fbSizeX = 0;
	this->fbSizeY = 0;
}

IRSensor::~IRSensor()
{
}

bool IRSensor::init(DMA2D_HandleTypeDef* dma2dHandler, uint8_t layer, const uint32_t fb_addr, const uint16_t fbSizeX, const uint16_t fbSizeY, const uint8_t* colorScheme)
{
    setFbAddress(fb_addr);
	this->dma2dHandler = dma2dHandler;
	this->layer = layer;
	this->setColorScheme(colorScheme);
	this->setFbSize(fbSizeX, fbSizeY);

    uint16_t statusRegister = I2Cx_ReadData16(MLX90640_ADDR, 0x8000);
    uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    uint16_t controlRegister2 = I2Cx_ReadData16(MLX90640_ADDR, 0x800F);

    //read MLX info
	readSerialNumber();
    if (mlxSerialNumber[0] == 0)
    {
	    return false;
    }

    setADCResolution(MLX90640_ADC_18BIT);
    setRefreshRate(MLX90640_16_HZ);
    setMlxMode(MLX90640_CHESS);

    mlx90640_refreshrate refreshRate = readRefreshRate();
    mlx90640_resolution_t adcResolution = readADCResolution();
    mlx90640_mode_t mode = readMlxMode();

	readMlxEE();
    convertMlxEEToParams();

    return true;
}

void IRSensor::setColorScheme(const uint8_t* colorScheme)
{
	this->colorScheme = colorScheme;
}


void IRSensor::setFbAddress(const uint32_t fb_addr)
{
	this->fb_addr = fb_addr;
}

void IRSensor::setFbSize(const uint16_t fbSizeX, const uint16_t fbSizeY)
{
	this->fbSizeX = fbSizeX;
	this->fbSizeY = fbSizeY;
}


void IRSensor::readMlxEE()
{
	I2Cx_ReadBuffer16(MLX90640_ADDR, 0x2400, mlxEE, sizeof(mlxEE));
}

void IRSensor::convertMlxEEToParams()
{
	/* Vdd */

	int16_t kVdd = (mlxEE[51] & 0xFF00) >> 8;
    if(kVdd > 127)
    {
        kVdd = kVdd - 256;
    }
    kVdd = 32 * kVdd;
    int16_t vdd25 = mlxEE[51] & 0x00FF;
    vdd25 = ((vdd25 - 256) << 5) - 8192;
    
    mlxParams.kVdd = kVdd;
    mlxParams.vdd25 = vdd25;

	/* PTAT */
    float KvPTAT = (mlxEE[50] & 0xFC00) >> 10;
    if(KvPTAT > 31)
    {
        KvPTAT = KvPTAT - 64;
    }
    KvPTAT = KvPTAT/4096;
    int16_t KtPTAT = mlxEE[50] & 0x03FF;
    if(KtPTAT > 511)
    {
        KtPTAT = KtPTAT - 1024;
    }
    KtPTAT = KtPTAT/8;
	const int16_t vPTAT25 = mlxEE[49];
	const float alphaPTAT = (mlxEE[16] & 0xF000) / pow(2, (double)14) + 8.0f;
    
    mlxParams.KvPTAT = KvPTAT;
    mlxParams.KtPTAT = KtPTAT;    
    mlxParams.vPTAT25 = vPTAT25;
    mlxParams.alphaPTAT = alphaPTAT;

	/* Gain */
    int16_t gainEE = mlxEE[48];
    if(gainEE > 32767)
    {
        gainEE = gainEE - 65536;
    }
    
    mlxParams.gainEE = gainEE;

	/* Tgc */
    float tgc = mlxEE[60] & 0x00FF;
    if(tgc > 127)
    {
        tgc = tgc - 256;
    }
    tgc = tgc / 32.0f;
    
    mlxParams.tgc = tgc;

	/* Resolution */
	const uint8_t resolutionEE = (mlxEE[56] & 0x3000) >> 12;    
    
    mlxParams.resolutionEE = resolutionEE;

	/* KsTa */
    float KsTa = (mlxEE[60] & 0xFF00) >> 8;
    if(KsTa > 127)
    {
        KsTa = KsTa -256;
    }
    KsTa = KsTa / 8192.0f;
    
    mlxParams.KsTa = KsTa;

	/* KsTo */
    int8_t step = ((mlxEE[63] & 0x3000) >> 12) * 10;
    
    mlxParams.ct[0] = -40;
    mlxParams.ct[1] = 0;
    mlxParams.ct[2] = (mlxEE[63] & 0x00F0) >> 4;
    mlxParams.ct[3] = (mlxEE[63] & 0x0F00) >> 8;    
    
    mlxParams.ct[2] = mlxParams.ct[2]*step;
    mlxParams.ct[3] = mlxParams.ct[2] + mlxParams.ct[3]*step;
    mlxParams.ct[4] = 400;

    int KsToScale = (mlxEE[63] & 0x000F) + 8;
    KsToScale = 1 << KsToScale;
    
    mlxParams.ksTo[0] = mlxEE[61] & 0x00FF;
    mlxParams.ksTo[1] = (mlxEE[61] & 0xFF00) >> 8;
    mlxParams.ksTo[2] = mlxEE[62] & 0x00FF;
    mlxParams.ksTo[3] = (mlxEE[62] & 0xFF00) >> 8;      
    
    for(int i = 0; i < 4; i++)
    {
        if(mlxParams.ksTo[i] > 127)
        {
            mlxParams.ksTo[i] = mlxParams.ksTo[i] - 256;
        }
        mlxParams.ksTo[i] = mlxParams.ksTo[i] / KsToScale;
    } 
    
    mlxParams.ksTo[4] = -0.0002;

	/* Alpha */
	int accRow[24];
    int accColumn[32];
	float alphaTemp[768];

	const uint8_t accRemScale = mlxEE[32] & 0x000F;
	const uint8_t accColumnScale = (mlxEE[32] & 0x00F0) >> 4;
	const uint8_t accRowScale = (mlxEE[32] & 0x0F00) >> 8;
	uint8_t alphaScale = ((mlxEE[32] & 0xF000) >> 12) + 30;
	const int alphaRef = mlxEE[33];
    
	int p = 0;

	for(int i = 0; i < 6; i++)
    {
        p = i * 4;
        accRow[p + 0] = (mlxEE[34 + i] & 0x000F);
        accRow[p + 1] = (mlxEE[34 + i] & 0x00F0) >> 4;
        accRow[p + 2] = (mlxEE[34 + i] & 0x0F00) >> 8;
        accRow[p + 3] = (mlxEE[34 + i] & 0xF000) >> 12;
    }
    
    for(int i = 0; i < 24; i++)
    {
        if (accRow[i] > 7)
        {
            accRow[i] = accRow[i] - 16;
        }
    }
    
    for(int i = 0; i < 8; i++)
    {
        p = i * 4;
        accColumn[p + 0] = (mlxEE[40 + i] & 0x000F);
        accColumn[p + 1] = (mlxEE[40 + i] & 0x00F0) >> 4;
        accColumn[p + 2] = (mlxEE[40 + i] & 0x0F00) >> 8;
        accColumn[p + 3] = (mlxEE[40 + i] & 0xF000) >> 12;
    }
    
    for(int i = 0; i < 32; i ++)
    {
        if (accColumn[i] > 7)
        {
            accColumn[i] = accColumn[i] - 16;
        }
    }

    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            p = 32 * i +j;
            alphaTemp[p] = (mlxEE[64 + p] & 0x03F0) >> 4;
            if (alphaTemp[p] > 31)
            {
                alphaTemp[p] = alphaTemp[p] - 64;
            }
            alphaTemp[p] = alphaTemp[p]*(1 << accRemScale);
            alphaTemp[p] = (alphaRef + (accRow[i] << accRowScale) + (accColumn[j] << accColumnScale) + alphaTemp[p]);
            alphaTemp[p] = alphaTemp[p] / pow(2,(double)alphaScale);
            alphaTemp[p] = alphaTemp[p] - mlxParams.tgc * (mlxParams.cpAlpha[0] + mlxParams.cpAlpha[1])/2;
            alphaTemp[p] = SCALEALPHA/alphaTemp[p];
        }
    }

	float temp = alphaTemp[0];
    for(int i = 1; i < 768; i++)
    {
        if (alphaTemp[i] > temp)
        {
            temp = alphaTemp[i];
        }
    }

	alphaScale = 0;
    while(temp < 32768)
    {
        temp = temp*2;
        alphaScale = alphaScale + 1;
    } 
    
    for(int i = 0; i < 768; i++)
    {
        temp = alphaTemp[i] * pow(2,(double)alphaScale);        
        mlxParams.alpha[i] = (temp + 0.5);        
        
    } 

    mlxParams.alphaScale = alphaScale;

	/* Offset */
	int occRow[24];
    int occColumn[32];
    int po = 0;

	const uint8_t occRemScale = (mlxEE[16] & 0x000F);
	const uint8_t occColumnScale = (mlxEE[16] & 0x00F0) >> 4;
	const uint8_t occRowScale = (mlxEE[16] & 0x0F00) >> 8;
    int16_t offsetRef = mlxEE[17];
    if (offsetRef > 32767)
    {
        offsetRef = offsetRef - 65536;
    }
    
    for(int i = 0; i < 6; i++)
    {
        po = i * 4;
        occRow[po + 0] = (mlxEE[18 + i] & 0x000F);
        occRow[po + 1] = (mlxEE[18 + i] & 0x00F0) >> 4;
        occRow[po + 2] = (mlxEE[18 + i] & 0x0F00) >> 8;
        occRow[po + 3] = (mlxEE[18 + i] & 0xF000) >> 12;
    }
    
    for(int i = 0; i < 24; i++)
    {
        if (occRow[i] > 7)
        {
            occRow[i] = occRow[i] - 16;
        }
    }
    
    for(int i = 0; i < 8; i++)
    {
        po = i * 4;
        occColumn[po + 0] = (mlxEE[24 + i] & 0x000F);
        occColumn[po + 1] = (mlxEE[24 + i] & 0x00F0) >> 4;
        occColumn[po + 2] = (mlxEE[24 + i] & 0x0F00) >> 8;
        occColumn[po + 3] = (mlxEE[24 + i] & 0xF000) >> 12;
    }
    
    for(int i = 0; i < 32; i ++)
    {
        if (occColumn[i] > 7)
        {
            occColumn[i] = occColumn[i] - 16;
        }
    }

    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            po = 32 * i +j;
            mlxParams.offset[po] = (mlxEE[64 + po] & 0xFC00) >> 10;
            if (mlxParams.offset[po] > 31)
            {
                mlxParams.offset[po] = mlxParams.offset[po] - 64;
            }
            mlxParams.offset[po] = mlxParams.offset[po]*(1 << occRemScale);
            mlxParams.offset[po] = (offsetRef + (occRow[i] << occRowScale) + (occColumn[j] << occColumnScale) + mlxParams.offset[po]);
        }
    }

	/* KtaPixel */
	int pkp = 0;
    int8_t KtaRC[4];
    int8_t KtaRoCo;
    int8_t KtaRoCe;
    int8_t KtaReCo;
    int8_t KtaReCe;
    uint8_t ktaScale1;
    uint8_t ktaScale2;
    uint8_t split;
    float ktaTemp[768];
    float tempKta;
    
    KtaRoCo = (mlxEE[54] & 0xFF00) >> 8;
    if (KtaRoCo > 127)
    {
        KtaRoCo = KtaRoCo - 256;
    }
    KtaRC[0] = KtaRoCo;
    
    KtaReCo = (mlxEE[54] & 0x00FF);
    if (KtaReCo > 127)
    {
        KtaReCo = KtaReCo - 256;
    }
    KtaRC[2] = KtaReCo;
      
    KtaRoCe = (mlxEE[55] & 0xFF00) >> 8;
    if (KtaRoCe > 127)
    {
        KtaRoCe = KtaRoCe - 256;
    }
    KtaRC[1] = KtaRoCe;
      
    KtaReCe = (mlxEE[55] & 0x00FF);
    if (KtaReCe > 127)
    {
        KtaReCe = KtaReCe - 256;
    }
    KtaRC[3] = KtaReCe;
  
    ktaScale1 = ((mlxEE[56] & 0x00F0) >> 4) + 8;
    ktaScale2 = (mlxEE[56] & 0x000F);

    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            pkp = 32 * i +j;
            split = 2*(pkp/32 - (pkp/64)*2) + pkp % 2;
            ktaTemp[pkp] = (mlxEE[64 + pkp] & 0x000E) >> 1;
            if (ktaTemp[pkp] > 3)
            {
                ktaTemp[pkp] = ktaTemp[pkp] - 8;
            }
            ktaTemp[pkp] = ktaTemp[pkp] * (1 << ktaScale2);
            ktaTemp[pkp] = KtaRC[split] + ktaTemp[pkp];
            ktaTemp[pkp] = ktaTemp[pkp] / pow(2,(double)ktaScale1);
        }
    }
    
    tempKta = fabs(ktaTemp[0]);
    for(int i = 1; i < 768; i++)
    {
        if (fabs(ktaTemp[i]) > tempKta)
        {
            tempKta = fabs(ktaTemp[i]);
        }
    }
    
    ktaScale1 = 0;
    while(tempKta < 64)
    {
        tempKta = tempKta*2;
        ktaScale1 = ktaScale1 + 1;
    }    
     
    for(int i = 0; i < 768; i++)
    {
        tempKta = ktaTemp[i] * pow(2,(double)ktaScale1);
        if (tempKta < 0)
        {
            mlxParams.kta[i] = (tempKta - 0.5);
        }
        else
        {
            mlxParams.kta[i] = (tempKta + 0.5);
        }        
        
    } 
    
    mlxParams.ktaScale = ktaScale1;

    /* KvPixel */
    int pKvp = 0;
    int8_t KvT[4];
    int8_t KvRoCo;
    int8_t KvRoCe;
    int8_t KvReCo;
    int8_t KvReCe;
    uint8_t kvScale;
    uint8_t splitKvp;
    float kvTemp[768];
    float tempKvp;

    KvRoCo = (mlxEE[52] & 0xF000) >> 12;
    if (KvRoCo > 7)
    {
        KvRoCo = KvRoCo - 16;
    }
    KvT[0] = KvRoCo;
    
    KvReCo = (mlxEE[52] & 0x0F00) >> 8;
    if (KvReCo > 7)
    {
        KvReCo = KvReCo - 16;
    }
    KvT[2] = KvReCo;
      
    KvRoCe = (mlxEE[52] & 0x00F0) >> 4;
    if (KvRoCe > 7)
    {
        KvRoCe = KvRoCe - 16;
    }
    KvT[1] = KvRoCe;
      
    KvReCe = (mlxEE[52] & 0x000F);
    if (KvReCe > 7)
    {
        KvReCe = KvReCe - 16;
    }
    KvT[3] = KvReCe;
  
    kvScale = (mlxEE[56] & 0x0F00) >> 8;


    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            pKvp = 32 * i +j;
            splitKvp = 2*(pKvp/32 - (pKvp/64)*2) + pKvp%2;
            kvTemp[pKvp] = KvT[splitKvp];
            kvTemp[pKvp] = kvTemp[pKvp] / pow(2,(double)kvScale);
        }
    }
    
    tempKvp = fabs(kvTemp[0]);
    for(int i = 1; i < 768; i++)
    {
        if (fabs(kvTemp[i]) > tempKvp)
        {
            tempKvp = fabs(kvTemp[i]);
        }
    }
    
    kvScale = 0;
    while(tempKvp < 64)
    {
        tempKvp = tempKvp*2;
        kvScale = kvScale + 1;
    }    
     
    for(int i = 0; i < 768; i++)
    {
        tempKvp = kvTemp[i] * pow(2,(double)kvScale);
        if (tempKvp < 0)
        {
            mlxParams.kv[i] = (tempKvp - 0.5);
        }
        else
        {
            mlxParams.kv[i] = (tempKvp + 0.5);
        }        
        
    } 
    
    mlxParams.kvScale = kvScale;

    /* CP */
    float alphaSP[2];
    int16_t offsetSP[2];
    float cpKv;
    float cpKta;
    uint8_t alphaScaleCp;
    uint8_t ktaScale1Cp;
    uint8_t kvScaleCp;

    alphaScaleCp = ((mlxEE[32] & 0xF000) >> 12) + 27;
    
    offsetSP[0] = (mlxEE[58] & 0x03FF);
    if (offsetSP[0] > 511)
    {
        offsetSP[0] = offsetSP[0] - 1024;
    }
    
    offsetSP[1] = (mlxEE[58] & 0xFC00) >> 10;
    if (offsetSP[1] > 31)
    {
        offsetSP[1] = offsetSP[1] - 64;
    }
    offsetSP[1] = offsetSP[1] + offsetSP[0]; 
    
    alphaSP[0] = (mlxEE[57] & 0x03FF);
    if (alphaSP[0] > 511)
    {
        alphaSP[0] = alphaSP[0] - 1024;
    }
    alphaSP[0] = alphaSP[0] /  pow(2,(double)alphaScaleCp);
    
    alphaSP[1] = (mlxEE[57] & 0xFC00) >> 10;
    if (alphaSP[1] > 31)
    {
        alphaSP[1] = alphaSP[1] - 64;
    }
    alphaSP[1] = (1 + alphaSP[1]/128) * alphaSP[0];
    
    cpKta = (mlxEE[59] & 0x00FF);
    if (cpKta > 127)
    {
        cpKta = cpKta - 256;
    }
    ktaScale1Cp = ((mlxEE[56] & 0x00F0) >> 4) + 8;    
    mlxParams.cpKta = cpKta / pow(2,(double)ktaScale1Cp);
    
    cpKv = (mlxEE[59] & 0xFF00) >> 8;
    if (cpKv > 127)
    {
        cpKv = cpKv - 256;
    }
    kvScaleCp = (mlxEE[56] & 0x0F00) >> 8;
    mlxParams.cpKv = cpKv / pow(2,(double)kvScaleCp);
       
    mlxParams.cpAlpha[0] = alphaSP[0];
    mlxParams.cpAlpha[1] = alphaSP[1];
    mlxParams.cpOffset[0] = offsetSP[0];
    mlxParams.cpOffset[1] = offsetSP[1];

    /* CILC */
    float ilChessC[3];
    uint8_t calibrationModeEE;
    
    calibrationModeEE = (mlxEE[10] & 0x0800) >> 4;
    calibrationModeEE = calibrationModeEE ^ 0x80;

    ilChessC[0] = (mlxEE[53] & 0x003F);
    if (ilChessC[0] > 31)
    {
        ilChessC[0] = ilChessC[0] - 64;
    }
    ilChessC[0] = ilChessC[0] / 16.0f;
    
    ilChessC[1] = (mlxEE[53] & 0x07C0) >> 6;
    if (ilChessC[1] > 15)
    {
        ilChessC[1] = ilChessC[1] - 32;
    }
    ilChessC[1] = ilChessC[1] / 2.0f;
    
    ilChessC[2] = (mlxEE[53] & 0xF800) >> 11;
    if (ilChessC[2] > 15)
    {
        ilChessC[2] = ilChessC[2] - 32;
    }
    ilChessC[2] = ilChessC[2] / 8.0f;
    
    mlxParams.calibrationModeEE = calibrationModeEE;
    mlxParams.ilChessC[0] = ilChessC[0];
    mlxParams.ilChessC[1] = ilChessC[1];
    mlxParams.ilChessC[2] = ilChessC[2];

    /* Deviation pixels */
    uint16_t pixCnt = 0;
    uint16_t brokenPixCnt = 0;
    uint16_t outlierPixCnt = 0;
    
    for(pixCnt = 0; pixCnt<20; pixCnt++)
    {
        mlxParams.brokenPixels[pixCnt] = 0xFFFF;
        mlxParams.outlierPixels[pixCnt] = 0xFFFF;
    }
        
    pixCnt = 0;    
    while (pixCnt < 768 && brokenPixCnt < 20 && outlierPixCnt < 20)
    {
        if(mlxEE[pixCnt+64] == 0)
        {
            mlxParams.brokenPixels[brokenPixCnt] = pixCnt;
            brokenPixCnt = brokenPixCnt + 1;
        }    
        else if((mlxEE[pixCnt+64] & 0x0001) != 0)
        {
            mlxParams.outlierPixels[outlierPixCnt] = pixCnt;
            outlierPixCnt = outlierPixCnt + 1;
        }    
        pixCnt++;
        
    } 
}

uint16_t* IRSensor::readSerialNumber()
{
	mlxSerialNumber[0] = I2Cx_ReadData16(MLX90640_ADDR, MLX90640_DEVICEID1);
	mlxSerialNumber[1] = I2Cx_ReadData16(MLX90640_ADDR, MLX90640_DEVICEID2);
	mlxSerialNumber[2] = I2Cx_ReadData16(MLX90640_ADDR, MLX90640_DEVICEID3);
	return mlxSerialNumber;
}

mlx90640_refreshrate_t IRSensor::readRefreshRate()
{
    const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    return (mlx90640_refreshrate_t)((controlRegister1 & 0x0380) >> 7);
}

void IRSensor::setRefreshRate(mlx90640_refreshrate_t rate)
{
    uint16_t value = (rate & 0x07) << 7;
    
    const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    value = (controlRegister1 & 0xFC7F) | value;
	I2Cx_WriteData16(MLX90640_ADDR, 0x800D, value);
}

mlx90640_resolution_t IRSensor::readADCResolution()
{
    const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    return (mlx90640_resolution_t)((controlRegister1 & 0x0C00) >> 10);
}

void IRSensor::setADCResolution(mlx90640_resolution_t resolution)
{
    uint16_t value = (resolution & 0x03) << 10;
    
    const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    value = (controlRegister1 & 0xF3FF) | value;
	I2Cx_WriteData16(MLX90640_ADDR, 0x800D, value);
}

mlx90640_mode_t IRSensor::readMlxMode()
{
    const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    return (mlx90640_mode_t)((controlRegister1 & 0x1000) >> 12);
}

void IRSensor::setMlxMode(mlx90640_mode_t mode)
{
	const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    uint16_t value = controlRegister1;

	if (mode == MLX90640_CHESS)
    {
		value = (controlRegister1 | 0x1000);
    }
    else if (mode == MLX90640_INTERLEAVED)
    {
		value = (controlRegister1 & 0xEFFF);
    }

    I2Cx_WriteData16(MLX90640_ADDR, 0x800D, value);
}

bool IRSensor::isFrameReady()
{
    uint16_t statusRegister = I2Cx_ReadData16(MLX90640_ADDR, 0x8000);
    return (bool)(statusRegister & 0x0008);
}

uint8_t IRSensor::IsPixelBad(uint16_t index)
{
    for(uint8_t i = 0; i < 5; i++)
    {
        if(index == mlxParams.outlierPixels[i] || index == mlxParams.brokenPixels[i])
        {
            return 1;
        }    
    }   
    
    return 0;
}

uint8_t IRSensor::CheckAdjacentPixels(uint16_t pix1, uint16_t pix2)
{
	const int16_t pixPosDif = pix1 - pix2;
     if(pixPosDif > -34 && pixPosDif < -30)
     {
         return 1;
     } 
     if(pixPosDif > -2 && pixPosDif < 2)
     {
         return 1;
     } 
     if(pixPosDif > 30 && pixPosDif < 34)
     {
         return 1;
     }
     
     return 0;  
}

void IRSensor::readImage(float emissivity)
{
    const float SUPPLY_VOLTAGE = 3.3;
	const uint8_t MAX_ATTEMPS = 5;
    uint16_t statusRegister = 0;
	uint16_t isReady = 1;
	uint8_t cnt = 0;

    statusRegister = I2Cx_ReadData16(MLX90640_ADDR, 0x8000);
    if ((statusRegister & 0x0008) == 0)
    {
	    return; //exit if data not ready
    }

    I2Cx_ReadBuffer16(MLX90640_ADDR, 0x0400, frameData, 832 * 2); //Read meas data
    // I2Cx_WriteData16(MLX90640_ADDR, 0x8000, statusRegister & 0xFFF7); //Clear bit “New data available in RAM” - Bit3 in 0x8000

    const uint16_t controlRegister1 = I2Cx_ReadData16(MLX90640_ADDR, 0x800D);
    frameData[832] = controlRegister1;
    frameData[833] = statusRegister & 0x0001;

    /* Vdd */
    float _vdd = frameData[810];
    if(_vdd > 32767)
    {
        _vdd = _vdd - 65536;
    }
    const int resolutionRAM = (frameData[832] & 0x0C00) >> 10;
    const float resolutionCorrection = pow(2, (double)mlxParams.resolutionEE) / pow(2, (double)resolutionRAM);
    _vdd = (resolutionCorrection * _vdd - mlxParams.vdd25) / mlxParams.kVdd + SUPPLY_VOLTAGE;
    this->vdd = _vdd;

    float ptat = frameData[800];
    if(ptat > 32767)
    {
        ptat = ptat - 65536;
    }

    float ptatArt = frameData[768];
    if(ptatArt > 32767)
    {
        ptatArt = ptatArt - 65536;
    }
    ptatArt = (ptat / (ptat * mlxParams.alphaPTAT + ptatArt)) * pow(2, 18.0);

    float _ta = (ptatArt / (1 + mlxParams.KvPTAT * (vdd - SUPPLY_VOLTAGE)) - mlxParams.vPTAT25);
    _ta = _ta / mlxParams.KtPTAT + 25;
    this->ta = _ta;

    float tr = this->ta - OPENAIR_TA_SHIFT;

	calculateTempMap(emissivity, tr);
    // calculateImageMap();
}

void IRSensor::calculateTempMap(float emissivity, float tr)
{
	float irDataCP[2];
	int8_t pattern;
	float alphaCorrR[4];
    int8_t range;

	const uint16_t subPage = frameData[833];
    
    float ta4 = (ta + 273.15f);
    ta4 = ta4 * ta4;
    ta4 = ta4 * ta4;
    float tr4 = (tr + 273.15f);
    tr4 = tr4 * tr4;
    tr4 = tr4 * tr4;
	const float taTr = tr4 - (tr4 - ta4) / emissivity;

	const float ktaScale = pow(2, (double)mlxParams.ktaScale);
	const float kvScale = pow(2, (double)mlxParams.kvScale);
	const float alphaScale = pow(2, (double)mlxParams.alphaScale);
    
    alphaCorrR[0] = 1 / (1 + mlxParams.ksTo[0] * 40);
    alphaCorrR[1] = 1 ;
    alphaCorrR[2] = (1 + mlxParams.ksTo[1] * mlxParams.ct[2]);
    alphaCorrR[3] = alphaCorrR[2] * (1 + mlxParams.ksTo[2] * (mlxParams.ct[3] - mlxParams.ct[2]));
    
//------------------------- Gain calculation -----------------------------------    
    float gain = frameData[778];
    if(gain > 32767)
    {
        gain = gain - 65536;
    }
    
    gain = mlxParams.gainEE / gain; 
  
//------------------------- To calculation -------------------------------------    
	const uint8_t mode = (frameData[832] & 0x1000) >> 5;
    
    irDataCP[0] = frameData[776];  
    irDataCP[1] = frameData[808];
    for( int i = 0; i < 2; i++)
    {
        if(irDataCP[i] > 32767)
        {
            irDataCP[i] = irDataCP[i] - 65536;
        }
        irDataCP[i] = irDataCP[i] * gain;
    }
    irDataCP[0] = irDataCP[0] - mlxParams.cpOffset[0] * (1 + mlxParams.cpKta * (ta - 25)) * (1 + mlxParams.cpKv * (vdd - 3.3));
    if( mode ==  mlxParams.calibrationModeEE)
    {
        irDataCP[1] = irDataCP[1] - mlxParams.cpOffset[1] * (1 + mlxParams.cpKta * (ta - 25)) * (1 + mlxParams.cpKv * (vdd - 3.3));
    }
    else
    {
      irDataCP[1] = irDataCP[1] - (mlxParams.cpOffset[1] + mlxParams.ilChessC[0]) * (1 + mlxParams.cpKta * (ta - 25)) * (1 + mlxParams.cpKv * (vdd - 3.3));
    }

    for(uint16_t pixelNumber = 0; pixelNumber < 768; pixelNumber++)
    {
	    const int8_t ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2;
	    const int8_t chessPattern = ilPattern ^ (pixelNumber - (pixelNumber / 2) * 2);
	    const int8_t conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);
        
        if(mode == 0)
        {
          pattern = ilPattern; 
        }
        else 
        {
          pattern = chessPattern; 
        }               
        
        if(pattern == frameData[833])
        {    
            float irData = frameData[pixelNumber];
            if(irData > 32767)
            {
                irData = irData - 65536;
            }
            irData = irData * gain;

            const float kta = mlxParams.kta[pixelNumber] / ktaScale;
            const float kv = mlxParams.kv[pixelNumber] / kvScale;
            irData = irData - mlxParams.offset[pixelNumber]*(1 + kta*(ta - 25))*(1 + kv*(vdd - 3.3));
            
            if(mode !=  mlxParams.calibrationModeEE)
            {
              irData = irData + mlxParams.ilChessC[2] * (2 * ilPattern - 1) - mlxParams.ilChessC[1] * conversionPattern; 
            }                       
    
            irData = irData - mlxParams.tgc * irDataCP[subPage];
            irData = irData / emissivity;
            
            float alphaCompensated = SCALEALPHA * alphaScale / mlxParams.alpha[pixelNumber];
            alphaCompensated = alphaCompensated*(1 + mlxParams.KsTa * (ta - 25));
                        
            float Sx = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
            Sx = sqrt(sqrt(Sx)) * mlxParams.ksTo[1];            
            
            float To = sqrt(sqrt(irData / (alphaCompensated * (1 - mlxParams.ksTo[1] * 273.15) + Sx) + taTr)) - 273.15;                     
                    
            if(To < mlxParams.ct[1])
            {
                range = 0;
            }
            else if(To < mlxParams.ct[2])   
            {
                range = 1;            
            }   
            else if(To < mlxParams.ct[3])
            {
                range = 2;            
            }
            else
            {
                range = 3;            
            }      
            
            To = sqrt(sqrt(irData / (alphaCompensated * alphaCorrR[range] * (1 + mlxParams.ksTo[range] * (To - mlxParams.ct[range]))) + taTr)) - 273.15;
                        
            dots[pixelNumber] = To;
        }
    }
}

void IRSensor::calculateImageMap()
{
	float irDataCP[2];
    float irData;
    float alphaCompensated;
	int8_t ilPattern;
    int8_t chessPattern;
    int8_t pattern;
    int8_t conversionPattern;
    float image;
    float kta;
    float kv;
    
    uint16_t subPage = frameData[833];
    
    float ktaScale = pow(2, (double)mlxParams.ktaScale);
    float kvScale = pow(2, (double)mlxParams.kvScale);
    
//------------------------- Gain calculation -----------------------------------    
    float gain = frameData[778];
    if(gain > 32767)
    {
        gain = gain - 65536;
    }
    
    gain = mlxParams.gainEE / gain; 
  
//------------------------- Image calculation -------------------------------------    
    uint8_t mode = (frameData[832] & 0x1000) >> 5;
    
    irDataCP[0] = frameData[776];  
    irDataCP[1] = frameData[808];
    for( int i = 0; i < 2; i++)
    {
        if(irDataCP[i] > 32767)
        {
            irDataCP[i] = irDataCP[i] - 65536;
        }
        irDataCP[i] = irDataCP[i] * gain;
    }
    irDataCP[0] = irDataCP[0] - mlxParams.cpOffset[0] * (1 + mlxParams.cpKta * (ta - 25)) * (1 + mlxParams.cpKv * (vdd - 3.3));
    if( mode == mlxParams.calibrationModeEE)
    {
        irDataCP[1] = irDataCP[1] - mlxParams.cpOffset[1] * (1 + mlxParams.cpKta * (ta - 25)) * (1 + mlxParams.cpKv * (vdd - 3.3));
    }
    else
    {
      irDataCP[1] = irDataCP[1] - (mlxParams.cpOffset[1] + mlxParams.ilChessC[0]) * (1 + mlxParams.cpKta * (ta - 25)) * (1 + mlxParams.cpKv * (vdd - 3.3));
    }

    for( int pixelNumber = 0; pixelNumber < 768; pixelNumber++)
    {
        ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2; 
        chessPattern = ilPattern ^ (pixelNumber - (pixelNumber/2)*2); 
        conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);
        
        if(mode == 0)
        {
          pattern = ilPattern; 
        }
        else 
        {
          pattern = chessPattern; 
        }
        
        if(pattern == frameData[833])
        {    
            irData = frameData[pixelNumber];
            if(irData > 32767)
            {
                irData = irData - 65536;
            }
            irData = irData * gain;
            
            kta = mlxParams.kta[pixelNumber]/ktaScale;
            kv = mlxParams.kv[pixelNumber]/kvScale;
            irData = irData - mlxParams.offset[pixelNumber]*(1 + kta*(ta - 25))*(1 + kv*(vdd - 3.3));

            if(mode !=  mlxParams.calibrationModeEE)
            {
              irData = irData + mlxParams.ilChessC[2] * (2 * ilPattern - 1) - mlxParams.ilChessC[1] * conversionPattern; 
            }
            
            irData = irData - mlxParams.tgc * irDataCP[subPage];
                        
            alphaCompensated = mlxParams.alpha[pixelNumber];
            
            image = irData*alphaCompensated;
            
            colors[pixelNumber] = image;
        }
    }
}

uint16_t IRSensor::getSubPage()
{
    return frameData[833];
}

float IRSensor::getVdd()
{
    return this->vdd;
}

float IRSensor::getTa()
{
    return this->ta;
}

float* IRSensor::getTempMap()
{
	return this->dots;
}

float IRSensor::getMaxTemp()
{
	return this->maxTemp;
}

float IRSensor::getMinTemp()
{
	return this->minTemp;
}

uint16_t IRSensor::getHotDotIndex()
{
	return this->hotDotIndex;
}

uint16_t IRSensor::getColdDotIndex()
{
	return this->coldDotIndex;
}

void IRSensor::drawGradient(const uint8_t startX, const uint8_t startY, const uint8_t stopX, const uint8_t stopY)
{
	const uint8_t height = stopY - startY;
	const uint8_t width = stopX - startX;
	const float diff = (maxTemp + minTempCorr - minTemp + maxTempCorr) / height;
	uint16_t line[240];
	for (uint8_t j = 0; j < height; j++)
	{
		const float temp = minTemp + (diff * j);
		line[j] = temperatureToRGB565(temp, minTemp + minTempCorr, maxTemp + maxTempCorr);	
	}

	for (uint8_t i = 0; i < width; i++)
	{
		volatile uint16_t *pSdramAddress = (uint16_t *)(this->fb_addr + (240 * (startX + i) + startY) * 2);
		for (uint8_t j = 0; j < height; j++)
		{
			*(volatile uint16_t *)pSdramAddress = line[j];	
			pSdramAddress++;
		}
	}
}

void IRSensor::visualizeImage(const uint8_t scale, const uint8_t method)
{
	uint8_t col = 0;
	uint8_t row = 0;
	uint16_t pixelIdx;
	uint16_t color;

	volatile uint16_t* pSdramAddress = (uint16_t *)this->fb_addr;

	_isImageReady = false;

	if (method == 0)
	{
        for (uint16_t i = 0; i < 32 * 24; i++)
		{
			colors[i] = this->temperatureToRGB565(dots[i], minTemp + minTempCorr, maxTemp + maxTempCorr);		
		}

		col = 32;
		while(col > 0)
		{
            for(uint8_t k = 0; k < scale; k++)
            {
	            row = 24;
				while(row > 0)
				{
	                pixelIdx = ((row - 1) * 32) + (col - 1);
	                for(uint8_t j = 0; j < scale; j++) {
		                *(volatile uint16_t *)pSdramAddress = colors[pixelIdx];
		                pSdramAddress++;
	                }
					row--;
				}
                pSdramAddress += (240 - (24 * scale));
            }
			col--;
		}
	}
	else if (method == 1)
	{
		float tmp, u, t, d1, d2, d3, d4;
		float p1, p2, p3, p4;

        for (uint16_t i = 0; i < 32 * 24; i++)
		{
			colors[i] = this->temperatureToRGB565(dots[i], minTemp + minTempCorr, maxTemp + maxTempCorr);		
		}

        col = 32 * scale;
		while(col > 0)
		{
            t = col / (float)scale;
			int16_t x = (int16_t)t;
            if (x >= 31)
            {
	            x--;
            }
            t = t - x;

			row = 24 * scale;
			while(row > 0)
			{
				u = row / (float)scale;
				int16_t y = (int16_t)u;
	            if (y >= 23)
	            {
		            y--;
	            }
	            u = u - y;

                d1 = (1 - t) * (1 - u);
				d2 = t * (1 - u);
				d3 = t * u;
				d4 = (1 - t) * u;

				p1 = dots[y * 32 + x];
				p2 = dots[y * 32 + x + 1];
				p3 = dots[(y + 1) * 32 + x + 1];
				p4 = dots[(y + 1) * 32 + x];

				float interp = p1*d1 + p2*d2 + p3*d3 + p4*d4;

				// pixelIdx = (y * 32) + x;

                *(volatile uint16_t *)pSdramAddress = this->temperatureToRGB565(interp, minTemp + minTempCorr, maxTemp + maxTempCorr);
	            pSdramAddress++;

				row--;
			}
            pSdramAddress += (240 - (24 * scale));
			col--;
		}
		
		/*for (uint16_t j = 0; j < resY; j++) {
			tmp = (float)(j) / (float)(resY - 1) * (8 - 1);
			int16_t h = (int16_t)tmp;
			if (h >= 8 - 1) {
				h = 8 - 2;
			}
			u = tmp - h;

			pSdramAddress = (uint16_t *)(this->fb_addr + (j * resX) * 2);

			for (uint16_t i = 0; i < resX; i++) {
				tmp = (float)(i) / (float)(resX - 1) * (8 - 1);
				int16_t w = (int16_t)tmp;
				if (w >= 8 - 1) {
					w = 8 - 2;
				}
				t = tmp - w;

				d1 = (1 - t) * (1 - u);
				d2 = t * (1 - u);
				d3 = t * u;
				d4 = (1 - t) * u;

				p1 = dots[h*8+w];
				p2 = dots[h * 8 + w + 1];
				p3 = dots[(h + 1) * 8 + w + 1];
				p4 = dots[(h + 1) * 8 + w];

				float temp = p1*d1 + p2*d2 + p3*d3 + p4*d4;

				*(volatile uint16_t *)pSdramAddress = this->temperatureToRGB565(temp, minTemp + minTempCorr, maxTemp + maxTempCorr);
				pSdramAddress++;
			}
		}*/
	}

	_isImageReady = true;
}

bool IRSensor::isImageReady()
{
	return this->_isImageReady;
}

void IRSensor::Dma2dXferCpltCallback(DMA2D_HandleTypeDef* hdma2d)
{

}

void IRSensor::findMinAndMaxTemp()
{
	this->minTemp = 1000;
	this->maxTemp = -100;
	for (uint16_t i = 0; i < 32 * 24; i++)
	{
		if (dots[i] < minTemp)
		{
			minTemp = dots[i];	
			coldDotIndex = i;
		}
		if (dots[i] > maxTemp)
		{
			maxTemp = dots[i];
			hotDotIndex = i;
		}
	}
}

uint16_t IRSensor::rgb2color(const uint8_t R, const uint8_t G, const uint8_t B)
{
	return ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3);
}

uint8_t IRSensor::calculateRGB(const uint8_t rgb1, const uint8_t rgb2, const float t1, const float step, const float t) {
	return (uint8_t)(rgb1 + (((t - t1) / step) * (rgb2 - rgb1)));
}

uint16_t IRSensor::temperatureToRGB565(const float temperature, const float minTemp, const float maxTemp) {
	uint16_t val;
	if (temperature < minTemp) {
		val = rgb2color(colorScheme[0], colorScheme[1], colorScheme[2]);
	}
	else if (temperature >= maxTemp) {
		const uint16_t colorSchemeSize = sizeof(DEFAULT_COLOR_SCHEME)/3;
		val = rgb2color(colorScheme[(colorSchemeSize - 1) * 3 + 0], colorScheme[(colorSchemeSize - 1) * 3 + 1], colorScheme[(colorSchemeSize - 1) * 3 + 2]);
	}
	else {
		const float step = (maxTemp - minTemp) / 10.0;
		const uint8_t step1 = (uint8_t)((temperature - minTemp) / step);
		const uint8_t step2 = step1 + 1;
		const uint8_t red = calculateRGB(colorScheme[step1 * 3 + 0], colorScheme[step2 * 3 + 0], (minTemp + step1 * step), step, temperature);
		const uint8_t green = calculateRGB(colorScheme[step1 * 3 + 1], colorScheme[step2 * 3 + 1], (minTemp + step1 * step), step, temperature);
		const uint8_t blue = calculateRGB(colorScheme[step1 * 3 + 2], colorScheme[step2 * 3 + 2], (minTemp + step1 * step), step, temperature);
		val = rgb2color(red, green, blue);
	}
	return val;
}
