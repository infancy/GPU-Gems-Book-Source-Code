//----------------------------------------------------------------------------------
// File:   SoftParticles.cpp
// Author: Tristan Lorach
// Email:  sdkfeedback@nvidia.com
// 
// Copyright (c) 2007 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA OR ITS SUPPLIERS
// BE  LIABLE  FOR  ANY  SPECIAL,  INCIDENTAL,  INDIRECT,  OR  CONSEQUENTIAL DAMAGES
// WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS OF BUSINESS PROFITS,
// BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS)
// ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//
//----------------------------------------------------------------------------------

#pragma warning (disable: 4995) // remove deprecated functions warning...

#include "DXUT.h"
#include "DXUTmisc.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "sdkmesh_old.h"
#include "resource.h"

#include "NVUTSkybox.h"

#include <fstream>
#include <vector>

#pragma warning (disable: 4996) // remove 'declared deprecated' functions warning...

// new class to make RenderSubset public instead of protected
class CDXUTMesh10Ex : public CDXUTMesh10
{
public:
    void RenderSubset( ID3D10EffectTechnique *pTechnique, 
                       UINT pass, 
                       ID3D10EffectShaderResourceVariable* ptxDiffuse,
                       ID3D10EffectVectorVariable* pvDiffuse, 
                       ID3D10EffectVectorVariable* pvSpecular, 
                       DWORD dwSubset )
    {
        CDXUTMesh10::RenderSubset( pTechnique, pass, ptxDiffuse, pvDiffuse, pvSpecular, dwSubset );
    }
};
CDXUTDialogResourceManager          g_DialogResourceManager;// manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

ID3DX10Font*                        g_pFont                 = NULL;         // Font for drawing text
ID3DX10Sprite*                      g_pSprite               = NULL;       // Sprite for batching text drawing

CDXUTMesh10Ex                       g_Mesh;
ID3D10EffectShaderResourceVariable* g_pSceneTextureShaderVariable = NULL;

//Depth Stencil as a texture
ID3D10Texture2D*                    g_pDSTTexture           = NULL;
ID3D10ShaderResourceView*           g_pDSTResView           = NULL;
ID3D10DepthStencilView*             g_pDSTView              = NULL;

ID3D10Buffer*                       g_VertexBuffer          = NULL;
ID3D10Texture2D*                    g_pRTTexture2DDepth     = NULL;
ID3D10RenderTargetView*             g_pRTViewDepth          = NULL;
ID3D10ShaderResourceView*           g_pTextureDepthView     = NULL;
ID3D10Texture2D*                    g_pRTTexture2DDepth_res = NULL; // for MSAA : a solution is to
ID3D10ShaderResourceView*           g_pTextureDepthView_res = NULL; // resolve the MSAA RT
ID3D10InputLayout*                  g_pVertexLayout         = NULL;
ID3D10InputLayout*                  g_pVertexLayoutSSprite  = NULL;
ID3D10Effect*                       g_pEffect               = NULL;
int                                 g_curTechnique          = 0;
int                                 g_debugParticleLoop     = 1;

float                               g_ClearColor[]          = {0.0f, 0.0f, 0.4f, 1.0f};
float                               g_ClearColor2[]         = {1000.0f, 1000.0f, 1000.0f, 1000.0f};

bool                                g_DebugTex              = false;
bool                                g_Animate               = true;
bool                                g_Initialised           = false;
bool                                g_ShowEffectParams      = false;
bool                                g_Rotate                = true;
bool                                g_bDrawSkyBox           = true;
bool								g_InitialDeviceCreation = false;

float                               g_SoftParticleScale     = 1.0;
float                               g_SoftParticleContrast  = 2.0;
float                               g_SoftParticleEpsilon   = 0.0f;
float                               g_maxSZ                 = 5.0;
float                               g_Alpha                 = 0.0f;
float                               g_nearFadeOut           = 1.0f;

NVUTSkybox                          g_Skybox;
// Environment map for the skybox
ID3D10Texture2D *                   g_EnvMap                = NULL;                    
ID3D10ShaderResourceView *          g_EnvMapSRV             = NULL;

struct Vertex
{
    D3DXVECTOR3 position;
    //D3DXVECTOR4 color; // Note, for now .X loader doesn't read color per vtx... :(
    D3DXVECTOR3 normal;
    D3DXVECTOR2 texcoord;
};
struct VertexSSprite
{
    D3DXVECTOR3 position;
    D3DXVECTOR2 parms;
};

#define NPARTICLES 10
#define PLIFETIME 20.0f
struct Particle
{
    float           lifetime;
    float           lifetimeStart;
    D3DXVECTOR3     vel;
    VertexSSprite   sprite;
};
std::vector<Particle>                   g_particles;
int                                     g_numParticles;

//
// Define vertex data layout
//
const D3D10_INPUT_ELEMENT_DESC g_layout[] =
{
    { "position",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(Vertex, position), D3D10_INPUT_PER_VERTEX_DATA, 0 },
    //NA in .X Reader{ "color",      0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, offsetof(Vertex, color), D3D10_INPUT_PER_VERTEX_DATA, 0 },
    { "normal",     0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(Vertex, normal), D3D10_INPUT_PER_VERTEX_DATA, 0 },
    { "texcoord",   0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(Vertex, texcoord), D3D10_INPUT_PER_VERTEX_DATA, 0 },
};
UINT g_numElementsScene = sizeof(g_layout)/sizeof(g_layout[0]);
const D3D10_INPUT_ELEMENT_DESC g_layoutSSprite[] =
{
    { "position",   0, DXGI_FORMAT_R32G32B32_FLOAT,     0, offsetof(VertexSSprite, position), D3D10_INPUT_PER_VERTEX_DATA, 0 },
    { "texcoord",   0, DXGI_FORMAT_R32G32_FLOAT,        0, offsetof(VertexSSprite, parms), D3D10_INPUT_PER_VERTEX_DATA, 0 },
};
UINT g_numElementsSSprite = sizeof(g_layoutSSprite)/sizeof(g_layoutSSprite[0]);

class CModelViewerCamera_Ex : public CModelViewerCamera
{ public:
    float GetNear() { return m_fNearPlane; }
    float GetFar()  { return m_fFarPlane;  } 
};
CModelViewerCamera_Ex    g_camera;

void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
HRESULT CreateEffect(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
HRESULT InitBackBufferDependentData(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);

/*----------------------------------------------
  --
  -- Message display
  --
  ----------------------------------------------*/
#define LOGMSG      PrintMessage // could be printf...
#define LOG_MSG     0
#define LOG_WARN    1
#define LOG_ERR     2
void PrintMessage(int level, LPCWSTR fmt, ...)
{
    static WCHAR dest[400];
    LPCWSTR *ppc = (LPCWSTR*)_ADDRESSOF(fmt);
    wvsprintf(dest, fmt, (va_list)&(ppc[1]));
    if(level == LOG_ERR) MessageBox(NULL, dest, L"Error", MB_OK|MB_ICONERROR);
    if(level == LOG_WARN) OutputDebugString(L"WARNING: ");
    OutputDebugString(dest);
    OutputDebugString(L"\n");
}

/*----------------------------------------------
  --
  -- UI IDs
  --
  ----------------------------------------------*/
#define IDC_TOGGLEFULLSCREEN          1
#define IDC_TOGGLEREF                 2
#define IDC_CHANGEDEVICE              3
#define IDC_CONTRAST                  4
#define IDC_SCALE                     5
#define IDC_TECH                      6
#define IDC_MAXSZ                     7
#define IDC_ANIMATE                   8
#define IDC_DEBUGTEX                  9
#define IDC_EPSILON                   10
#define IDC_LOOP                      11
#define IDC_ROTATE                    12
#define IDC_NEARFADEOUT               13
/*----------------------------------------------
    --
    -- Misc helpers
    --
    ----------------------------------------------*/
namespace Tools 
{
    WCHAR                      g_path[MAX_PATH+1];
    char                       g_pathb[MAX_PATH+1];
    inline const char * PathNameAsBS(const char * filename)
    {
        HRESULT hr;
        WCHAR wfname[MAX_PATH];
        if(!filename)
            return NULL;
        mbstowcs_s(NULL, wfname, MAX_PATH, filename, MAX_PATH);
        hr = ::DXUTFindDXSDKMediaFileCch(Tools::g_path, MAX_PATH, wfname);
        if(FAILED(hr))
        {
            LOGMSG(LOG_ERR,L"couldn't find %s. Maybe wrong path...", filename);
            return NULL;
        }
        wcstombs_s(NULL, Tools::g_pathb, MAX_PATH, Tools::g_path, MAX_PATH);
        return Tools::g_pathb;
    }
    inline WCHAR * PathNameAsWS(LPWSTR filename)
    {
        HRESULT hr;
        if(!filename)
            return NULL;
        hr = ::DXUTFindDXSDKMediaFileCch(g_path, MAX_PATH, filename);
        if(FAILED(hr))
        {
            LOGMSG(LOG_ERR,L"couldn't find %s. Maybe wrong path...", filename);
            return NULL;
        }
        return g_path;
    }
    //--------------------------------------------------------------------------------------
    // GetPassDesc
    //--------------------------------------------------------------------------------------
    D3D10_PASS_DESC g_tmpPassDesc;
    D3D10_PASS_DESC * GetPassDesc(ID3D10Effect *pEffect, LPCSTR technique, LPCSTR pass)
    {
        ID3D10EffectTechnique * m_Tech = NULL;
        if(HIWORD(technique))
        m_Tech = pEffect->GetTechniqueByName(technique);
        else
        m_Tech = pEffect->GetTechniqueByIndex((UINT)LOWORD(technique));
        if(HIWORD(pass))
        {
            if ( FAILED( m_Tech->GetPassByName(pass)->GetDesc(&Tools::g_tmpPassDesc) ) )
            {
                LOGMSG(LOG_ERR, L"Failed getting description\n" );
                return NULL;
            }
        } else
        {
            if ( FAILED( m_Tech->GetPassByIndex((UINT)LOWORD(pass))->GetDesc(&Tools::g_tmpPassDesc) ) )
            {
                LOGMSG(LOG_ERR, L"Failed getting description\n" );
                return NULL;
            }
        }
        return &Tools::g_tmpPassDesc;
    }
    HRESULT CreateCubeTextureFromFile(LPCSTR texfile, ID3D10Texture2D **ppTex, ID3D10ShaderResourceView **ppView, ID3D10Device* pd3dDevice)
    {
      HRESULT hr = S_OK;
        ID3D10Texture2D *pTex;
        ID3D10ShaderResourceView *pView;
        ID3D10Resource *pTexture;
        D3DX10_IMAGE_LOAD_INFO LoadInfo;
        D3DX10_IMAGE_INFO      SrcInfo;
        D3DX10GetImageInfoFromFileA(Tools::PathNameAsBS(texfile), NULL, &SrcInfo, &hr);

        LoadInfo.Width          = SrcInfo.Width;
        LoadInfo.Height         = SrcInfo.Height;
        LoadInfo.Depth          = SrcInfo.Depth;
        LoadInfo.FirstMipLevel  = 0;
        LoadInfo.MipLevels      = SrcInfo.MipLevels;
        LoadInfo.Usage          = D3D10_USAGE_DEFAULT;
        LoadInfo.BindFlags      = D3D10_BIND_SHADER_RESOURCE;
        LoadInfo.CpuAccessFlags = 0;
        LoadInfo.MiscFlags      = SrcInfo.MiscFlags;
        LoadInfo.Format         = SrcInfo.Format;
        LoadInfo.Filter         = D3DX10_FILTER_LINEAR;
        LoadInfo.MipFilter      = D3DX10_FILTER_LINEAR;
        LoadInfo.pSrcInfo       = &SrcInfo;

        D3DX10CreateTextureFromFileA( pd3dDevice, Tools::PathNameAsBS(texfile), &LoadInfo, NULL, &pTexture, &hr);
        if(FAILED(hr))
          return hr;
        D3D10_RESOURCE_DIMENSION d;
        pTexture->GetType(&d);
        hr = pTexture->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&pTex);
        pTexture->Release();
        if(FAILED(hr))
        {
            LOGMSG(LOG_ERR,L"Error in QueryInterface(ID3D10Texture2D) for %S", Tools::PathNameAsBS(texfile));
            return hr;
        }
        D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
        viewDesc.Format = LoadInfo.Format;
        viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
        viewDesc.Texture2D.MostDetailedMip = LoadInfo.FirstMipLevel;
        viewDesc.Texture2D.MipLevels = LoadInfo.MipLevels;
        if ( FAILED( pd3dDevice->CreateShaderResourceView( pTexture, &viewDesc, &pView) ) )
        {
            LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for %S", Tools::PathNameAsBS(texfile));
            if(ppTex) *ppTex = pTex;
            else pTex->Release();
            return hr;
        }
      LOGMSG(LOG_MSG, L"Created texture and view for %S", Tools::PathNameAsBS(texfile));
      if(ppTex) *ppTex = pTex;
      else pTex->Release();
      if(ppView) *ppView = pView;
      else pView->Release();
      return S_OK;
    }
}// Tools
/*----------------------------------------------
  --
  --
  --
  ----------------------------------------------*/
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{    
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN: DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:        DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:     g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_MAXSZ:
        {
            WCHAR sz[100];
            g_maxSZ = (float) (g_SampleUI.GetSlider( IDC_MAXSZ )->GetValue()* 0.01f);
            StringCchPrintf( sz, 100, L"MaxSz : %0.2f", g_maxSZ); 
            g_SampleUI.GetStatic( IDC_MAXSZ )->SetText( sz );
            g_pEffect->GetVariableByName("maxSZ")->AsScalar()->SetFloat(g_maxSZ);
            break;
        }
        case IDC_SCALE:
        {
            WCHAR sz[100];
            g_SoftParticleScale = (float) (g_SampleUI.GetSlider( IDC_SCALE )->GetValue()* 0.001f);
            StringCchPrintf( sz, 100, L"Scale : %0.2f", g_SoftParticleScale); 
            g_SampleUI.GetStatic( IDC_SCALE )->SetText( sz );
            g_pEffect->GetVariableByName("SoftParticleScale")->AsScalar()->SetFloat(g_SoftParticleScale);
            break;
        }
        case IDC_CONTRAST:
        {
            WCHAR sz[100];
            g_SoftParticleContrast = (float) (g_SampleUI.GetSlider( IDC_CONTRAST )->GetValue()* 0.001f);
            StringCchPrintf( sz, 100, L"Contrast : %0.2f", g_SoftParticleContrast); 
            g_SampleUI.GetStatic( IDC_CONTRAST )->SetText( sz );
            g_pEffect->GetVariableByName("SoftParticleContrast")->AsScalar()->SetFloat(g_SoftParticleContrast);
            break;
        }
        case IDC_EPSILON:
        {
            WCHAR sz[100];
            g_SoftParticleEpsilon = (float) (g_SampleUI.GetSlider( IDC_EPSILON )->GetValue()* 0.001f);
            StringCchPrintf( sz, 100, L"Epsilon Threshold : %0.3f", g_SoftParticleEpsilon); 
            g_SampleUI.GetStatic( IDC_EPSILON )->SetText( sz );
            g_pEffect->GetVariableByName("zEpsilon")->AsScalar()->SetFloat(g_SoftParticleEpsilon);
            break;
        }
        case IDC_LOOP:
        {
            WCHAR sz[100];
            g_debugParticleLoop = (g_SampleUI.GetSlider( IDC_LOOP )->GetValue());
            StringCchPrintf( sz, 100, L"debug Loop : %d", g_debugParticleLoop); 
            g_SampleUI.GetStatic( IDC_LOOP )->SetText( sz );
            if(g_debugParticleLoop > 1)
            {
                g_Rotate = false;
                g_SampleUI.GetCheckBox( IDC_ROTATE )->SetChecked(false);
                g_DebugTex = true;
                g_SampleUI.GetCheckBox( IDC_DEBUGTEX )->SetChecked(true);
                g_pEffect->GetVariableByName("debugTex")->AsScalar()->SetBool(g_DebugTex);
            }
            if(g_maxSZ > 1.0)
            {
                g_maxSZ = 5.0;
                StringCchPrintf( sz, 100, L"MaxSz : %0.2f", g_maxSZ); 
                g_SampleUI.GetStatic( IDC_MAXSZ )->SetText( sz );
                g_pEffect->GetVariableByName("maxSZ")->AsScalar()->SetFloat(g_maxSZ);
            }
            break;
        }
        case IDC_NEARFADEOUT:
        {
            WCHAR sz[100];
            g_nearFadeOut = (g_SampleUI.GetSlider( IDC_NEARFADEOUT )->GetValue())* 0.001f;
            StringCchPrintf( sz, 100, L"Near fade out : %0.2f", g_nearFadeOut); 
            g_SampleUI.GetStatic( IDC_NEARFADEOUT )->SetText( sz );
            float v4[4];
            v4[0] = (g_nearFadeOut * g_camera.GetFar() / (g_camera.GetFar() - g_camera.GetNear()));
            v4[1] = g_nearFadeOut;
            v4[2] = v4[3] = 0.0f;
            if( FAILED( g_pEffect->GetVariableByName("NearFadeout")->AsVector()->SetFloatVector(v4) ) )
            {
                LOGMSG(LOG_WARN, L"update of DepthRTSz failed" );
            }
            break;
        }
        case IDC_ANIMATE:
        {
            g_Animate = g_SampleUI.GetCheckBox( IDC_ANIMATE )->GetChecked();
            break;
        }
        case IDC_ROTATE:
        {
            g_Rotate = g_SampleUI.GetCheckBox( IDC_ROTATE )->GetChecked();
            break;
        }
        case IDC_DEBUGTEX:
        {
            g_DebugTex = g_SampleUI.GetCheckBox( IDC_DEBUGTEX )->GetChecked();
            g_pEffect->GetVariableByName("debugTex")->AsScalar()->SetBool(g_DebugTex);
            if(g_DebugTex)
            {
                g_Rotate = false;
                g_SampleUI.GetCheckBox( IDC_ROTATE )->SetChecked(false);
            }
            break;
        }
        case IDC_TECH:
        {
            g_curTechnique = (int)g_SampleUI.GetComboBox(IDC_TECH)->GetSelectedIndex();
            break;
        }
    }
}
/*----------------------------------------------
  --
  -- Create the controls
  --
  ----------------------------------------------*/
void CreateUI(int width)
{
    WCHAR sz[100];
    g_HUD.RemoveAllControls();
    g_HUD.SetCallback( OnGUIEvent ); 
    int iY = 10; 
    int iX = width - 150;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", iX+15, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", iX+15, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", iX+15, iY += 24, 125, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); 
    g_SampleUI.RemoveAllControls();

    g_SampleUI.AddComboBox(IDC_TECH, iX+15, iY += 46, 130, 22);

    StringCchPrintf( sz, 100, L"Particle size : %0.2f", g_maxSZ); 
    g_SampleUI.AddStatic( IDC_MAXSZ, sz, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_MAXSZ, iX+15, iY += 24, 100, 22, 0, 1500, (int)(g_maxSZ*100) );

    StringCchPrintf( sz, 100, L"Contrast : %0.2f", g_SoftParticleContrast); 
    g_SampleUI.AddStatic( IDC_CONTRAST, sz, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_CONTRAST, iX+15, iY += 24, 100, 22, 1000, 5000, (int)(g_SoftParticleContrast*1000) );

    StringCchPrintf( sz, 100, L"Scale : %0.2f", g_SoftParticleScale); 
    g_SampleUI.AddStatic( IDC_SCALE, sz, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_SCALE, iX+15, iY += 24, 100, 22, 0, 4000, (int)(g_SoftParticleScale*1000) );

    StringCchPrintf( sz, 100, L"Threshold : %0.3f", g_SoftParticleEpsilon); 
    g_SampleUI.AddStatic( IDC_EPSILON, sz, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_EPSILON, iX+15, iY += 24, 100, 22, 0, 100, (int)(g_SoftParticleEpsilon*1000.0f) );

    StringCchPrintf( sz, 100, L"Near fade out : %0.2f", g_nearFadeOut); 
    g_SampleUI.AddStatic( IDC_NEARFADEOUT, sz, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_NEARFADEOUT, iX+15, iY += 24, 100, 22, 0, 5000, (int)(g_nearFadeOut*1000.0f) );

    g_SampleUI.AddCheckBox(IDC_ANIMATE, L"Animate", iX+15, iY += 24, 150, 22, g_Animate);
    g_SampleUI.AddCheckBox(IDC_ROTATE, L"Rotate View", iX+15, iY += 24, 150, 22, g_Rotate);
    g_SampleUI.AddCheckBox(IDC_DEBUGTEX, L"Debug Texture", iX+15, iY += 24, 150, 22, g_DebugTex);

#if 0
    //This option is for testing the performances : it allows to repeat many times the same set of similar particles
    // to see how the HW is performing. Canceled by default
    StringCchPrintf( sz, 100, L"debug Loop : %d", g_debugParticleLoop); 
    g_SampleUI.AddStatic( IDC_LOOP, sz, iX, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_LOOP, iX+15, iY += 24, 100, 22, 1, 400, g_debugParticleLoop );
#endif
    //
    // techniques in combo
    //
    if(g_pEffect)
    {
        D3D10_EFFECT_DESC effectdesc;
        g_pEffect->GetDesc(&effectdesc);
        for(unsigned int i=0; i<effectdesc.Techniques;i++)
        {
          ID3D10EffectTechnique *tech = g_pEffect->GetTechniqueByIndex(i);
          if(!tech)
            break;
          D3D10_TECHNIQUE_DESC desc;
          tech->GetDesc(&desc);
          WCHAR sz[100];
          StringCchPrintf( sz, 100, L"%S", desc.Name); 
          g_SampleUI.GetComboBox(IDC_TECH)->AddItem(sz, NULL);
          if(!strcmp(desc.Name, "MRT_MSAA"))
          {
              LOGMSG(LOG_MSG, L"setting DepthFromSecondRT as the default technique" );
              g_curTechnique = i;
          }
        }
        g_SampleUI.GetComboBox(IDC_TECH)->SetSelectedByIndex(g_curTechnique);

    }
}
/*----------------------------------------------
  --
  --
  --
  ----------------------------------------------*/
HRESULT CreateTexture1DFromFile(LPCSTR texfile, ID3D10Texture1D **ppTex, ID3D10ShaderResourceView **ppView, ID3D10Device* pd3dDevice)
{
    HRESULT hr = S_OK;
    ID3D10Texture1D *pTex;
    ID3D10ShaderResourceView *pView;
    ID3D10Resource *pTexture;
    D3DX10_IMAGE_LOAD_INFO LoadInfo;
    D3DX10_IMAGE_INFO      SrcInfo;
    D3DX10GetImageInfoFromFileA(Tools::PathNameAsBS(texfile), NULL, &SrcInfo, &hr);

    LoadInfo.Width          = SrcInfo.Width;
    LoadInfo.Height         = SrcInfo.Height;
    LoadInfo.Depth          = SrcInfo.Depth;
    LoadInfo.FirstMipLevel  = 0;
    LoadInfo.MipLevels      = SrcInfo.MipLevels;
    LoadInfo.Usage          = D3D10_USAGE_DEFAULT;
    LoadInfo.BindFlags      = D3D10_BIND_SHADER_RESOURCE;
    LoadInfo.CpuAccessFlags = 0;
    LoadInfo.MiscFlags      = SrcInfo.MiscFlags;
    LoadInfo.Format         = SrcInfo.Format;
    LoadInfo.Filter         = D3DX10_FILTER_LINEAR;
    LoadInfo.MipFilter      = D3DX10_FILTER_LINEAR;
    LoadInfo.pSrcInfo       = &SrcInfo;

    D3DX10CreateTextureFromFileA( pd3dDevice, Tools::PathNameAsBS(texfile), &LoadInfo, NULL, &pTexture, &hr);
    if(FAILED(hr))
      return hr;
    D3D10_RESOURCE_DIMENSION d;
    pTexture->GetType(&d);
    hr = pTexture->QueryInterface(__uuidof(ID3D10Texture1D), (void**)&pTex);
    pTexture->Release();
    if(FAILED(hr))
    {
        LOGMSG(LOG_ERR,L"Error in QueryInterface(ID3D10Texture1D) for %s", Tools::PathNameAsBS(texfile));
        return hr;
    }
    D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
    viewDesc.Format = LoadInfo.Format;
    viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE1D;
    viewDesc.Texture2D.MostDetailedMip = LoadInfo.FirstMipLevel;
    viewDesc.Texture2D.MipLevels = LoadInfo.MipLevels;
    if ( FAILED( pd3dDevice->CreateShaderResourceView( pTexture, &viewDesc, &pView) ) )
    {
        LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for %s", Tools::PathNameAsBS(texfile));
        if(ppTex) *ppTex = pTex;
        else pTex->Release();
        return hr;
    }
    LOGMSG(LOG_MSG, L"Created texture and view for %s", Tools::PathNameAsBS(texfile));
    if(ppTex) *ppTex = pTex;
    else pTex->Release();
    if(ppView) *ppView = pView;
    else pView->Release();
    return hr;
}
/*----------------------------------------------
  --
  --
  --
  ----------------------------------------------*/
HRESULT CreateTexture2DFromFile(LPCSTR texfile, ID3D10Texture2D **ppTex, ID3D10ShaderResourceView **ppView, ID3D10Device* pd3dDevice)
{
    HRESULT hr = S_OK;
    ID3D10Texture2D *pTex;
    ID3D10ShaderResourceView *pView;
    ID3D10Resource *pTexture;
    D3DX10_IMAGE_LOAD_INFO LoadInfo;
    D3DX10_IMAGE_INFO      SrcInfo;
    D3DX10GetImageInfoFromFileA(Tools::PathNameAsBS(texfile), NULL, &SrcInfo, &hr);

    LoadInfo.Width          = SrcInfo.Width;
    LoadInfo.Height         = SrcInfo.Height;
    LoadInfo.Depth          = SrcInfo.Depth;
    LoadInfo.FirstMipLevel  = 0;
    LoadInfo.MipLevels      = SrcInfo.MipLevels;
    LoadInfo.Usage          = D3D10_USAGE_DEFAULT;
    LoadInfo.BindFlags      = D3D10_BIND_SHADER_RESOURCE;
    LoadInfo.CpuAccessFlags = 0;
    LoadInfo.MiscFlags      = SrcInfo.MiscFlags;
    LoadInfo.Format         = SrcInfo.Format;
    LoadInfo.Filter         = D3DX10_FILTER_LINEAR;
    LoadInfo.MipFilter      = D3DX10_FILTER_LINEAR;
    LoadInfo.pSrcInfo       = &SrcInfo;

    D3DX10CreateTextureFromFileA( pd3dDevice, Tools::PathNameAsBS(texfile), &LoadInfo, NULL, &pTexture, &hr);
    if(FAILED(hr))
      return hr;
    D3D10_RESOURCE_DIMENSION d;
    pTexture->GetType(&d);
    hr = pTexture->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&pTex);
    pTexture->Release();
    if(FAILED(hr))
    {
        LOGMSG(LOG_ERR,L"Error in QueryInterface(ID3D10Texture2D) for %s", Tools::PathNameAsBS(texfile));
        return hr;
    }
    D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
    viewDesc.Format = LoadInfo.Format;
    viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MostDetailedMip = LoadInfo.FirstMipLevel;
    viewDesc.Texture2D.MipLevels = LoadInfo.MipLevels;
    if ( FAILED( pd3dDevice->CreateShaderResourceView( pTexture, &viewDesc, &pView) ) )
    {
        LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for %s", Tools::PathNameAsBS(texfile));
        if(ppTex) *ppTex = pTex;
        else pTex->Release();
        return hr;
    }
    LOGMSG(LOG_MSG, L"Created texture and view for %S", Tools::PathNameAsBS(texfile));
    if(ppTex) *ppTex = pTex;
    else pTex->Release();
    if(ppView) *ppView = pView;
    else pView->Release();
    return hr;
}
HRESULT CreateTexture3DFromFile(LPCSTR texfile, ID3D10Texture3D **ppTex, ID3D10ShaderResourceView **ppView, ID3D10Device* pd3dDevice)
{
    HRESULT hr = S_OK;
    ID3D10Texture3D *pTex;
    ID3D10ShaderResourceView *pView;

    ID3D10Resource *pTexture;
    D3DX10_IMAGE_LOAD_INFO LoadInfo;
    D3DX10_IMAGE_INFO      SrcInfo;
    D3DX10GetImageInfoFromFileA(Tools::PathNameAsBS(texfile), NULL, &SrcInfo, &hr);

    LoadInfo.Width          = SrcInfo.Width;
    LoadInfo.Height         = SrcInfo.Height;
    LoadInfo.Depth          = SrcInfo.Depth;
    LoadInfo.FirstMipLevel  = 0;
    LoadInfo.MipLevels      = SrcInfo.MipLevels;
    LoadInfo.Usage          = D3D10_USAGE_DEFAULT;
    LoadInfo.BindFlags      = D3D10_BIND_SHADER_RESOURCE;
    LoadInfo.CpuAccessFlags = 0;
    LoadInfo.MiscFlags      = SrcInfo.MiscFlags;
    LoadInfo.Format         = SrcInfo.Format;
    LoadInfo.Filter         = D3DX10_FILTER_LINEAR;
    LoadInfo.MipFilter      = D3DX10_FILTER_LINEAR;
    LoadInfo.pSrcInfo       = &SrcInfo;

    D3DX10CreateTextureFromFileA( pd3dDevice, Tools::PathNameAsBS(texfile), &LoadInfo, NULL, &pTexture, &hr);
    if(FAILED(hr))
      return hr;
    D3D10_RESOURCE_DIMENSION d;
    pTexture->GetType(&d);
    hr = pTexture->QueryInterface(__uuidof(ID3D10Texture3D), (void**)&pTex);
    pTexture->Release();
    if(FAILED(hr))
    {
        LOGMSG(LOG_ERR,L"Error in QueryInterface(ID3D10Texture3D) for %s", Tools::PathNameAsBS(texfile));
        return hr;
    }
    D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
    viewDesc.Format = LoadInfo.Format;
    viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE3D;
    viewDesc.Texture2D.MostDetailedMip = LoadInfo.FirstMipLevel;
    viewDesc.Texture2D.MipLevels = LoadInfo.MipLevels;
    if ( FAILED( pd3dDevice->CreateShaderResourceView( pTexture, &viewDesc, &pView) ) )
    {
        LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for %S", Tools::PathNameAsBS(texfile));
        if(ppTex) *ppTex = pTex;
        else pTex->Release();
        return hr;
    }

    LOGMSG(LOG_MSG, L"Created texture and view for %S", Tools::PathNameAsBS(texfile));
    if(ppTex) *ppTex = pTex;
    else pTex->Release();
    if(ppView) *ppView = pView;
    else pView->Release();
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Load texture that we found in the effect : semantic "name"
//--------------------------------------------------------------------------------------
HRESULT LoadEffectTextures(ID3D10Effect* pEffect, ID3D10Device* pd3dDevice)
{
    HRESULT h     = S_OK;
    HRESULT hrRet = S_OK;
    for(UINT i=0, hr = S_OK; !FAILED(hr); i++)
    {
        ID3D10EffectVariable *v = pEffect->GetVariableByIndex(i);
        if(v == NULL)
            break;
        D3D10_EFFECT_TYPE_DESC d;
        hr = v->GetType()->GetDesc(&d);
        ID3D10EffectVariable *f = v->GetAnnotationByName("file");
        if(!f)
        {
            LOGMSG(LOG_WARN, L"no 'file' annotation for the texture");
            continue;
        }
        //
        // Yeah sorry : we take a mbs, search with wcs and is it as mbs...
        //
        const char * pFname = NULL;
        ID3D10ShaderResourceView * texView = NULL;
        if(d.Type == D3D10_SVT_TEXTURE1D)
        {
            if( FAILED( h=f->AsString()->GetString(&pFname) ) )
            {
                hrRet = h;
                LOGMSG(LOG_WARN, L"cannot get the name of the 'file' semantic" );
            }
            else if( FAILED( h=CreateTexture1DFromFile(pFname, NULL, &texView, pd3dDevice) ) )
            {
                hrRet = h;
                LOGMSG(LOG_ERR, L"failed to load texture %S", pFname );
            }
            else if( texView && FAILED( h=v->AsShaderResource()->SetResource(texView) ) )
            {
                hrRet = h;
                LOGMSG(LOG_ERR, L"failed to bind texture_face with DDS image" );
            }
            SAFE_RELEASE(texView);
        }
        else if(d.Type == D3D10_SVT_TEXTURE2D)
        {
            if( FAILED( h=f->AsString()->GetString(&pFname) ) )
            {
                hrRet = h;
                LOGMSG(LOG_WARN, L"cannot get the name of the 'file' semantic" );
            }
            else if( FAILED( h=CreateTexture2DFromFile(pFname, NULL, &texView, pd3dDevice) ) )
            {
                hrRet = h;
                LOGMSG(LOG_ERR, L"failed to load texture %S", pFname );
            }
            else if( texView && FAILED( h=v->AsShaderResource()->SetResource(texView) ) )
            {
                hrRet = h;
                LOGMSG(LOG_ERR, L"failed to bind texture_face with DDS image" );
            }
            SAFE_RELEASE(texView);
        }
        else if(d.Type == D3D10_SVT_TEXTURECUBE)
        {
        }
        else if(d.Type == D3D10_SVT_TEXTURE3D)
        {
            if( FAILED( h=f->AsString()->GetString(&pFname) ) )
            {
                hrRet = h;
                LOGMSG(LOG_WARN, L"cannot get the name of the 'file' semantic" );
            }
            else if( FAILED( h= CreateTexture3DFromFile(pFname, NULL, &texView, pd3dDevice) ) )
            {
                hrRet = h;
                LOGMSG(LOG_ERR, L"failed to load texture %S", pFname );
            }
            else if( FAILED( h=v->AsShaderResource()->SetResource(texView) ) )
            {
                hrRet = h;
                LOGMSG(LOG_ERR, L"failed to bind texture_face with DDS image" );
            }
            SAFE_RELEASE(texView);
        }
    }
    return hrRet;
}

/*----------------------------------------------
  --
  --
  --
  ----------------------------------------------*/
bool InitInputGeometry(ID3D10Device* pd3dDevice)
{
    HRESULT hr;
    if(g_pEffect)
        g_pSceneTextureShaderVariable = g_pEffect->GetVariableByName("texture_mesh")->AsShaderResource();
    //
    // get Signature and create the layout
    //
    if(g_pEffect)
    {
        D3D10_PASS_DESC passDesc;
        ID3D10EffectTechnique * pTech = NULL;
        pTech = g_pEffect->GetTechniqueByIndex(g_curTechnique);
        if( FAILED( pTech->GetPassByIndex(0)->GetDesc(&passDesc) ) )
        {
            LOGMSG(LOG_ERR, L"GetDesc() failed.\n" );
            return false;
        }
        if(  FAILED(  pd3dDevice->CreateInputLayout( g_layout, g_numElementsScene, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_pVertexLayout ) ) )
        {
            LOGMSG(LOG_ERR, L"CreateElementLayout() for pass 0 failed.\n" );
            return false;
        }

        if( FAILED( pTech->GetPassByIndex(1)->GetDesc(&passDesc) ) )
        {
            LOGMSG(LOG_ERR, L"GetDesc() failed.\n" );
            return false;
        }
        if(  FAILED(  pd3dDevice->CreateInputLayout( g_layoutSSprite, g_numElementsSSprite, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_pVertexLayoutSSprite ) ) )
        {
            LOGMSG(LOG_ERR, L"CreateElementLayout() for pass 1 failed.\n" );
            return false;
        }
        //
        // Simple DXUT Mesh
        //
        WCHAR stra[MAX_PATH];
        LPCWSTR objname = L"..\\..\\Media\\StHelen.x";
        if(FAILED(DXUTFindDXSDKMediaFileCch(stra, MAX_PATH, objname)))
        {
            LOGMSG(LOG_ERR, L"Failed to load %s", objname);
            return false;
        } else
        {
            g_Mesh.Destroy();
            hr = g_Mesh.Create( pd3dDevice,stra, (D3D10_INPUT_ELEMENT_DESC*)g_layout, g_numElementsScene );
        }
    }
    //
    // init particle array
    //
    for(int i=0; i < NPARTICLES; i++)
    {
        Particle p;
        p.lifetimeStart = (PLIFETIME/NPARTICLES) * (float)i;
        p.lifetime = p.lifetimeStart;
        p.vel = D3DXVECTOR3(0.0f,0.0f,1.0f);
        p.sprite.position = D3DXVECTOR3(0,0,0);
        p.sprite.parms = D3DXVECTOR2(0,0);
        g_particles.push_back(p);
    }
    //
    // Create a buffer of particles
    //
    SAFE_RELEASE(g_VertexBuffer);
    D3D10_BUFFER_DESC bdesc;
    bdesc.Usage = D3D10_USAGE_DYNAMIC;
    bdesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    bdesc.MiscFlags = 0;
    bdesc.ByteWidth = sizeof(VertexSSprite) * NPARTICLES;
    D3D10_SUBRESOURCE_DATA data;
    VertexSSprite v[NPARTICLES];
    for(int i=0; i< NPARTICLES; i++)
    {
        v[i]  = g_particles[i].sprite;
    }
    data.pSysMem            = v;
    data.SysMemPitch        = 0;
    data.SysMemSlicePitch   = 0;
    hr = pd3dDevice->CreateBuffer(&bdesc, &data, &g_VertexBuffer);
    if(FAILED(hr))
    {
        LOGMSG(LOG_ERR, L"Failed to create the vertex buffer for particles");
        return false;
    }

    return true;
}
//--------------------------------------------------------------------------------------
// init Depth Stencil as a texture
//--------------------------------------------------------------------------------------
HRESULT InitDSTBuffer(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
    //
    // Create the Depth stencil by hand instead of asking DXUT
    //
    ID3D10Texture2D           *pRTTexture2D;
    // Get back the texture of the backbuffer
    DXUTGetDXGISwapChain()->GetBuffer(0, __uuidof(*pRTTexture2D), reinterpret_cast<void**> ( &pRTTexture2D ) );
    D3D10_TEXTURE2D_DESC rtTexDesc;
    pRTTexture2D->GetDesc( &rtTexDesc ); // take some data from the basic RT
    SAFE_RELEASE(pRTTexture2D);
    rtTexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    rtTexDesc.MipLevels = 1;
    rtTexDesc.ArraySize = 1;
    rtTexDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
    // If we tell we want to use it as a Shader Resource when in MSAA, we will fail
    if(pBackBufferSurfaceDesc->SampleDesc.Count == 1)
        rtTexDesc.BindFlags |= D3D10_BIND_SHADER_RESOURCE;
    rtTexDesc.Usage = D3D10_USAGE_DEFAULT;
    rtTexDesc.CPUAccessFlags = 0;
    rtTexDesc.MiscFlags = 0;
    SAFE_RELEASE(g_pDSTTexture);
    if( FAILED( pd3dDevice->CreateTexture2D(&rtTexDesc, NULL, &g_pDSTTexture) ) )
    {
        //LOGMSG(LOG_ERR, "CreateTexture2D() failed.");
        return E_FAIL;
    }
    //
    // Create the View of the texture pRTTexture2DDepth
    // If MSAA is used, we cannot do this
    //
    if(pBackBufferSurfaceDesc->SampleDesc.Count == 1)
    {
        D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
        viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = 1;
        SAFE_RELEASE(g_pDSTResView);
        if ( FAILED( pd3dDevice->CreateShaderResourceView( g_pDSTTexture, &viewDesc, &g_pDSTResView) ) )
        {
            LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for DepthStencil");
            return E_FAIL;
        }
        LOGMSG(LOG_MSG, L"Created texture and view for pRTTexture2DDepth");
    }
    //
    // Create the Depth Stencil View
    //
    D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
    descDSV.Format = DXGI_FORMAT_D32_FLOAT;
    descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
    descDSV.Texture2D.MipSlice = 0;

    // Create the depth stencil view
    SAFE_RELEASE( g_pDSTView );
    if( FAILED( pd3dDevice->CreateDepthStencilView( g_pDSTTexture, // Depth stencil texture
                                             &descDSV, // Depth stencil desc
                                             &g_pDSTView ) ) )  // [out] Depth stencil view
    {
        LOGMSG(LOG_ERR,L"Error in CreateDepthStencilView");
        return E_FAIL;
    }
    LOGMSG(LOG_MSG, L"Created CreateDepthStencilView");

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    if(pDeviceSettings->d3d10.sd.SampleDesc.Count > 4)
    {
        pDeviceSettings->d3d10.sd.SampleDesc.Count   = 4;
        LOGMSG(LOG_ERR,L"More than 4 samples not supported yet\n"
            L"This would require to recompile the shader after we refefined 'NUMSAMPLES'...");
    }
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;
    g_Initialised = true;

       g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    CreateUI(pBackBufferSurfaceDesc->Width);

    V_RETURN( g_DialogResourceManager.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10CreateDevice( pd3dDevice ) );
    V_RETURN( D3DX10CreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                                L"Arial", &g_pFont ) );
    V_RETURN( D3DX10CreateSprite( pd3dDevice, 512, &g_pSprite ) );

    if(FAILED(hr = CreateEffect(pd3dDevice, pBackBufferSurfaceDesc)))
    {
        g_Initialised = false;
        return S_OK;
    }

    if(!InitInputGeometry(pd3dDevice))
    {
        g_Initialised = false;
        return S_OK;
    }

    LoadEffectTextures(g_pEffect, pd3dDevice);

    
    V_RETURN( g_Skybox.OnCreateDevice( pd3dDevice ) );

    // Load envmap for the skybox
    hr = Tools::CreateCubeTextureFromFile(Tools::PathNameAsBS("..\\..\\media\\SkyCubemap.dds"), &g_EnvMap, &g_EnvMapSRV, pd3dDevice);
    if(FAILED(hr))
    {
       LOGMSG(LOG_ERR, L"Failed to load Skybox map");
       return hr;
    }

    g_Skybox.SetTexture(g_EnvMapSRV);
    return hr;
}
//--------------------------------------------------------------------------------------
// init window-size dependent stuff
//--------------------------------------------------------------------------------------
HRESULT InitBackBufferDependentData(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
    float v4[4];
    if(!g_Initialised)
        return E_FAIL;
    //
    //Screen ration infos needed by the shader
    //
    v4[0] = g_camera.GetNear();
    v4[1] = g_camera.GetFar();
    v4[2] = v4[1] * v4[0];
    v4[3] = v4[1] - v4[0];
    if( FAILED( g_pEffect->GetVariableByName("near_far")->AsVector()->SetFloatVector(v4) ) )
    {
        LOGMSG(LOG_WARN, L"update of near_far failed" );
    }
    v4[0] = (float)pBackBufferSurfaceDesc->Width;
    v4[1] = (float)pBackBufferSurfaceDesc->Height;
    v4[2] = (float)pBackBufferSurfaceDesc->Width/(float)pBackBufferSurfaceDesc->Height;
    if( FAILED( g_pEffect->GetVariableByName("DepthRTSz")->AsVector()->SetFloatVector(v4) ) )
    {
        LOGMSG(LOG_WARN, L"update of DepthRTSz failed" );
    }
    v4[0] = (g_nearFadeOut * g_camera.GetFar() / (g_camera.GetFar() - g_camera.GetNear()));
    v4[1] = g_nearFadeOut;
    v4[2] = v4[3] = 0.0f;
    if( FAILED( g_pEffect->GetVariableByName("NearFadeout")->AsVector()->SetFloatVector(v4) ) )
    {
        LOGMSG(LOG_WARN, L"update of DepthRTSz failed" );
    }
    //
    // Second render target for depth
    //
    ID3D10Texture2D           *pRTTexture2D;
    DXUTGetDXGISwapChain()->GetBuffer(0, __uuidof(*pRTTexture2D), reinterpret_cast<void**> ( &pRTTexture2D ) );
    D3D10_TEXTURE2D_DESC rtTexDesc;
    pRTTexture2D->GetDesc( &rtTexDesc ); // take some data from the basic RT
    SAFE_RELEASE(pRTTexture2D);
    rtTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
    rtTexDesc.MipLevels = 1;
    rtTexDesc.ArraySize = 1;
    // we keep 'D3D10_BIND_SHADER_RESOURCE' because we also propose a version where
    // we resolve the MSAA in the shader
    rtTexDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE|D3D10_BIND_RENDER_TARGET;
    rtTexDesc.Usage = D3D10_USAGE_DEFAULT;
    rtTexDesc.CPUAccessFlags = 0;
    rtTexDesc.MiscFlags = 0;
    SAFE_RELEASE(g_pRTTexture2DDepth);
    if( FAILED( pd3dDevice->CreateTexture2D(&rtTexDesc, NULL, &g_pRTTexture2DDepth) ) )
    {
        LOGMSG(LOG_ERR, L"CreateTexture2D() failed.");
        return E_FAIL;
    }
    SAFE_RELEASE(g_pRTViewDepth);
    if( FAILED( pd3dDevice->CreateRenderTargetView(g_pRTTexture2DDepth, NULL, &g_pRTViewDepth) ) )
    {
        LOGMSG(LOG_ERR, L"CreateRenderTargetView() failed.");
        return E_FAIL;
    }
    //
    // Create the View of the texture pRTTexture2DDepth
    // In MSAA, this resource can still be passed to the shader for the mode where we want
    // to resolve MSAA in the shader.
    //
    D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
    viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
    viewDesc.ViewDimension = (rtTexDesc.SampleDesc.Count > 1) ? 
        D3D10_SRV_DIMENSION_TEXTURE2DMS : 
        D3D10_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MostDetailedMip = 0;
    viewDesc.Texture2D.MipLevels = 1;
    SAFE_RELEASE(g_pTextureDepthView);
    if ( FAILED( pd3dDevice->CreateShaderResourceView( g_pRTTexture2DDepth, &viewDesc, &g_pTextureDepthView) ) )
    {
        LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for Depth Render target");
        return E_FAIL;
    }
    LOGMSG(LOG_MSG, L"Created texture and view for pRTTexture2DDepth");
    //
    // in case of MSAA we want to expose a solution where we do a 'resolve' of the MSAA surface
    // here we declare the resolved surface to be used as a texture
    //
    SAFE_RELEASE(g_pRTTexture2DDepth_res);
    SAFE_RELEASE(g_pTextureDepthView_res);
    if(rtTexDesc.SampleDesc.Count > 1)
    {
        LOGMSG(LOG_MSG,L"MSAA to %d. Creating a render target to resolve MSAA RT...", rtTexDesc.SampleDesc.Count);
        rtTexDesc.Format = DXGI_FORMAT_R32_FLOAT;
        rtTexDesc.MipLevels = 1;
        rtTexDesc.ArraySize = 1;
        rtTexDesc.SampleDesc.Count = 1;
        rtTexDesc.SampleDesc.Quality = 0;
        rtTexDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE|D3D10_BIND_RENDER_TARGET; // TODO: remove D3D10_BIND_RENDER_TARGET
        rtTexDesc.Usage = D3D10_USAGE_DEFAULT;
        rtTexDesc.CPUAccessFlags = 0;
        rtTexDesc.MiscFlags = 0;
        if( FAILED( pd3dDevice->CreateTexture2D(&rtTexDesc, NULL, &g_pRTTexture2DDepth_res) ) )
        {
            LOGMSG(LOG_ERR, L"CreateTexture2D() failed.");
            return E_FAIL;
        }
        //
        // Create the shader resource View of the resolved texture
        //
        D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc;
        viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
        viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = 1;
        if ( FAILED( pd3dDevice->CreateShaderResourceView( g_pRTTexture2DDepth_res, &viewDesc, &g_pTextureDepthView_res) ) )
        {
            LOGMSG(LOG_ERR,L"Error in CreateShaderResourceView for Depth Render target");
            return E_FAIL;
        }
        LOGMSG(LOG_MSG, L"Created texture and view to resolve pRTTexture2DDepth");
    }
    return S_OK;
}
//--------------------------------------------------------------------------------------
// create the effect
//--------------------------------------------------------------------------------------
HRESULT CreateEffect(ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
    ID3D10Blob *pErrors = NULL;
    HRESULT hr;
    WCHAR path[MAX_PATH];

    DXUTSetMediaSearchPath(L"..\\Source\\SoftParticles\\");
    hr = DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"SoftParticles.fx");
    SAFE_RELEASE(g_pEffect);
    LOGMSG(LOG_MSG, L"loading effect..." );
    if( !FAILED(hr) )
    {
        //
        // Changing the samples does impact the compilation of the shader
        // the clean way to do the Job is to set a Macro so that we can
        // recompile the shader depending on how many samples are available
        // Note: we implemented this define. But we should recompile this
        // from 'OnD3D10ResizedSwapChain' callback. Which is not done here...
        // So the sample now only working for MSAA 4x (See ModifyDeviceSettings)
        //
        char numsamples[4];
        sprintf_s(numsamples, 4, "%d", pBackBufferSurfaceDesc->SampleDesc.Count);
        D3D10_SHADER_MACRO mac[2] = {"NUMSAMPLES", numsamples, NULL, NULL};
        hr = D3DX10CreateEffectFromFile(path, mac, NULL, "fx_4_0", D3D10_SHADER_NO_PRESHADER, 0, pd3dDevice, NULL, NULL, &g_pEffect, &pErrors, &hr);
    }

    if( FAILED( hr ) )
    {
        LOGMSG(LOG_ERR, L"Failed creating Effect from file" );
        if(pErrors) {
            LPCSTR szErrors = (LPCSTR)pErrors->GetBufferPointer();
            LOGMSG(LOG_ERR,L"%S", szErrors );
            pErrors->Release();
        }
        return hr;
    }
    else LOGMSG(LOG_MSG, L"Effect loaded" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;
    V_RETURN( g_DialogResourceManager.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D10ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    //
    // Update Camera
    //
    float fAspectRatio = (float)pBackBufferSurfaceDesc->Width / (float)pBackBufferSurfaceDesc->Height;
    g_camera.SetProjParams( D3DX_PI/4, fAspectRatio, 0.1f, 100.0f );
    g_camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

    //
    // Update things that are dependend on the window size
    //
    if(FAILED(InitBackBufferDependentData(pd3dDevice, pBackBufferSurfaceDesc)))
        g_Initialised = false;
    //
    // Re-create UI
    //
    CreateUI(pBackBufferSurfaceDesc->Width);
    //
    // Skybox
    //
    g_Skybox.OnResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );
    //
    // Init the depth stencil
    //
    return InitDSTBuffer(pd3dDevice, pBackBufferSurfaceDesc);
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    g_camera.FrameMove(fElapsedTime);
}

//--------------------------------------------------------------------------------------
// Render Using the ZBuffer as a texture - best method
//--------------------------------------------------------------------------------------
void RenderUsingZBufferAsTexture( ID3D10Device* pd3dDevice, float fTime, float fElapsedTime, void* pUserContext, D3DXMATRIX &mWorldViewProj )
{
    HRESULT hr;
    ID3D10RenderTargetView *pRTs[] = {DXUTGetD3D10RenderTargetView()};
    pd3dDevice->OMSetRenderTargets(1, pRTs, g_pDSTView);
    pd3dDevice->ClearDepthStencilView(g_pDSTView, D3D10_CLEAR_DEPTH, 1.0f, 0);
    pd3dDevice->ClearRenderTargetView( DXUTGetD3D10RenderTargetView(), g_ClearColor);
    //
    // Sky box
    //
    if(g_bDrawSkyBox)
        g_Skybox.OnFrameRender(mWorldViewProj, 0.8f, D3DXVECTOR4(60.0f/255.0f,109.0f/255.0f,29.0f/255.0f,1.0f));
    //
    // Render the landscape
    //
    ID3D10Buffer *pNullBuffers[] = { NULL };
    UINT offsets[2] = {0, 0};
    pd3dDevice->SOSetTargets( 1, pNullBuffers, offsets );

    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    g_Mesh.RenderSubset( g_pEffect->GetTechniqueByIndex(g_curTechnique), 0, g_pSceneTextureShaderVariable, NULL, NULL, 0 );
    //
    // we cannot use this technique in MSAA modes : display a message and that's it
    //
    if(g_pRTTexture2DDepth_res)
    {
        CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 15 );

        txtHelper.Begin();
        txtHelper.SetInsertionPos( 50, 300 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.4f, 1.0f ) );
        txtHelper.DrawTextLine( L"Cannot Use this technique in MSAA mode" );
        txtHelper.End();
        return;
    }
    //
    // Render the 2 Billboards
    //
    // We need to remove the the DST from the Render Targets if we want to use it as a texture :
    //
    pd3dDevice->OMSetRenderTargets(1, pRTs, NULL);
    ID3D10EffectPass *pass = g_pEffect->GetTechniqueByIndex(g_curTechnique)->GetPassByIndex(1);
    hr = g_pEffect->GetVariableByName("texture_ZBuffer")->AsShaderResource()->SetResource(g_pDSTResView);
    hr = g_pEffect->GetVariableByName("offsetZ")->AsScalar()->SetFloat(g_Animate ? fmodf(0.1f*fTime, 1.0f) : 0.3f);
    pd3dDevice->IASetInputLayout( g_pVertexLayoutSSprite );
    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    UINT stride = sizeof(VertexSSprite);
    UINT offset = 0;
    pd3dDevice->IASetVertexBuffers(0, 1, &g_VertexBuffer, &stride, &offset);
    pass->Apply(0);
    // Let's loop g_debugParticleLoop times to test performances
    for(int ii=0; ii<g_debugParticleLoop; ii++)
    {
        pd3dDevice->Draw(g_numParticles, 0);
    }    
    //
    // We need to unbind g_pDSTResView from texture_ZBuffer because this buffer
    // will be used later as the typical depth buffer, again
    // must call an annoying Apply(0) here : to flush SetResource(NULL)
    //
    hr = g_pEffect->GetVariableByName("texture_ZBuffer")->AsShaderResource()->SetResource(NULL);
    pass->Apply(0);
}

//--------------------------------------------------------------------------------------
// Render Using an additional render target, storing the Z values 'by hand'
//--------------------------------------------------------------------------------------
void RenderUsingFP32RTAsZBuffer( ID3D10Device* pd3dDevice, float fTime, float fElapsedTime, void* pUserContext, D3DXMATRIX &mWorldViewProj, bool bResolve )
{
    HRESULT hr;
    //
    // Here a 2nd render target will have the depth values.
    // it will also be MSAA if ever requested. If so, we will resolve it
    //
    ID3D10RenderTargetView *pRTs[] = {DXUTGetD3D10RenderTargetView(), g_pRTViewDepth};
    pd3dDevice->OMSetRenderTargets(2, pRTs, g_pDSTView);
    pd3dDevice->ClearDepthStencilView(g_pDSTView, D3D10_CLEAR_DEPTH, 1.0f, 0);
    pd3dDevice->ClearRenderTargetView( DXUTGetD3D10RenderTargetView(), g_ClearColor);
    g_ClearColor2[0] = g_camera.GetFar();
    pd3dDevice->ClearRenderTargetView( g_pRTViewDepth, g_ClearColor2);

    //
    // Sky box
    //
    if(g_bDrawSkyBox)
        g_Skybox.OnFrameRender(mWorldViewProj, 0.8f, D3DXVECTOR4(60.0f/255.0f,109.0f/255.0f,29.0f/255.0f,1.0f));
    //
    // Render the landscape
    //
    ID3D10Buffer *pNullBuffers[] = { NULL };
    UINT offsets[2] = {0, 0};
    pd3dDevice->SOSetTargets( 1, pNullBuffers, offsets );

    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    g_Mesh.RenderSubset( g_pEffect->GetTechniqueByIndex(g_curTechnique), 0, g_pSceneTextureShaderVariable, NULL, NULL, 0 );
    //
    // back to a single render target 
    //
    ID3D10RenderTargetView *pRTView[2] = {DXUTGetD3D10RenderTargetView(), NULL};
    pd3dDevice->OMSetRenderTargets(2, pRTView, g_pDSTView);
    //
    // Render the Billboards
    //
    ID3D10EffectPass *pass = g_pEffect->GetTechniqueByIndex(g_curTechnique)->GetPassByIndex(1);
    hr = g_pEffect->GetVariableByName("offsetZ")->AsScalar()->SetFloat(g_Animate ? fmodf(0.2f*fTime, 1.0f) : 0.3f);
    pd3dDevice->SOSetTargets( 0, pNullBuffers, offsets );
    //
    // Use either
    // 1- a resolved version of the R32Float Render target.
    // Meaning that values will be *averaged*. However we are ok thanks to soft particle approximation
    // 2- a non resolved, relying on the shader to do the Job
    // 3- basic mode when no MSAA set
    //
    if(bResolve && g_pRTTexture2DDepth_res) // we have MSAA because we create this intermediate
    {
        pd3dDevice->ResolveSubresource(g_pRTTexture2DDepth_res, 0,g_pRTTexture2DDepth, 0, DXGI_FORMAT_R32_FLOAT);
        hr = g_pEffect->GetVariableByName("texture_depth")->AsShaderResource()->SetResource(g_pTextureDepthView_res);
    } else if(g_pRTTexture2DDepth_res) { // we are still in MSAA but we don't want 'resolve'
        hr = g_pEffect->GetVariableByName("texture_depthMS")->AsShaderResource()->SetResource(g_pTextureDepthView);
    } else { // g_pRTTexture2DDepth_res == 0 meaning that we are not in any MSAA mode
        hr = g_pEffect->GetVariableByName("texture_depth")->AsShaderResource()->SetResource(g_pTextureDepthView);
    }
    pd3dDevice->IASetInputLayout( g_pVertexLayoutSSprite );
    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    UINT stride = sizeof(VertexSSprite);
    UINT offset = 0;
    pd3dDevice->IASetVertexBuffers(0, 1, &g_VertexBuffer, &stride, &offset);
    pass->Apply(0);
    // Let's loop g_debugParticleLoop times to test performances
    for(int ii=0; ii<g_debugParticleLoop; ii++)
    {
        pd3dDevice->Draw(g_numParticles, 0);
    }
    //
    // We need to unbind g_pTextureDepthView from texture_depth because this buffer
    // will be used later as a render target
    // must call an annoying Apply(0) here : to flush SetResource(NULL)
    //
    hr = g_pEffect->GetVariableByName("texture_depth")->AsShaderResource()->SetResource(NULL);
    pass->Apply(0);
}

//--------------------------------------------------------------------------------------
// Render Using nothing but Z test... to show the basic issue we want to solve
//--------------------------------------------------------------------------------------
void RenderSimple( ID3D10Device* pd3dDevice, float fTime, float fElapsedTime, void* pUserContext, D3DXMATRIX &mWorldViewProj )
{
    HRESULT hr;
    ID3D10RenderTargetView *pRTs[] = {DXUTGetD3D10RenderTargetView()};
    pd3dDevice->OMSetRenderTargets(1, pRTs, g_pDSTView);
    pd3dDevice->ClearDepthStencilView(g_pDSTView, D3D10_CLEAR_DEPTH, 1.0f, 0);
    pd3dDevice->ClearRenderTargetView( DXUTGetD3D10RenderTargetView(), g_ClearColor);

    //
    // Sky box
    //
    if(g_bDrawSkyBox)
        g_Skybox.OnFrameRender(mWorldViewProj, 0.8f, D3DXVECTOR4(60.0f/255.0f,109.0f/255.0f,29.0f/255.0f,1.0f));
    //
    // Render the landscape
    //
    ID3D10Buffer *pNullBuffers[] = { NULL };
    UINT offsets[2] = {0, 0};
    pd3dDevice->SOSetTargets( 1, pNullBuffers, offsets );

    pd3dDevice->IASetInputLayout( g_pVertexLayout );
    g_Mesh.RenderSubset( g_pEffect->GetTechniqueByIndex(g_curTechnique), 0, g_pSceneTextureShaderVariable, NULL, NULL, 0 );
    //
    // Render the 2 Billboards
    //
    ID3D10EffectPass *pass = g_pEffect->GetTechniqueByIndex(g_curTechnique)->GetPassByIndex(1);
    hr = g_pEffect->GetVariableByName("offsetZ")->AsScalar()->SetFloat(g_Animate ? fmodf(0.2f*fTime, 1.0f) : 0.3f);
    pd3dDevice->SOSetTargets( 0, pNullBuffers, offsets );

    pd3dDevice->IASetInputLayout( g_pVertexLayoutSSprite );
    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
    UINT stride = sizeof(VertexSSprite);
    UINT offset = 0;
    pd3dDevice->IASetVertexBuffers(0, 1, &g_VertexBuffer, &stride, &offset);
    pass->Apply(0);
    // Let's loop g_debugParticleLoop times to test performances
    for(int ii=0; ii<g_debugParticleLoop; ii++)
    {
        pd3dDevice->Draw(g_numParticles, 0);
    }
}
//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 15 );

    txtHelper.Begin();
    txtHelper.SetInsertionPos( 2, 0 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats(true) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
    txtHelper.End();
}
//--------------------------------------------------------------------------------------
// Update particles
//--------------------------------------------------------------------------------------
inline float frand()
{
    return (float)rand() / (float)RAND_MAX;
}
inline float srand()
{
    return 2.0f * (((float)rand() / (float)RAND_MAX)-0.5f);
}
void UpdateParticles(ID3D10Device* pd3dDevice, float fElapsedTime)
{
    HRESULT hr;
    //
    // Simple case of having a debug texture : draw one single quad
    //
    if(g_DebugTex)
    {
        VertexSSprite *v;
        if(SUCCEEDED(hr = g_VertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0,(LPVOID*)&v)))
        {
            v->position = D3DXVECTOR3(0,2.0,0);
            v->parms = D3DXVECTOR2(1,1);
            g_VertexBuffer->Unmap();
            g_numParticles = 1;
        } else {
            LOGMSG(LOG_WARN, L"Failed to Map vertex buffer\n");
        }
        return;
    }
    //
    // update the particles
    //
    for(int i=0; i < NPARTICLES; i++)
    {
        Particle &p = g_particles[i];
        p.lifetime -= fElapsedTime;
        if(p.lifetime < 0.0f)
        {
            // random vel-dir
            p.sprite.position = D3DXVECTOR3(10.0f * srand(), 1.0f + srand(),-10.0f);
            p.lifetimeStart = PLIFETIME - (0.2f * PLIFETIME * frand());
            p.lifetime = p.lifetimeStart;
            p.sprite.parms = D3DXVECTOR2(0, 1.0f - 0.5f * frand());
        } 
        else if(p.sprite.parms.y > 0.0) // only update/display if size > 0
        {
            p.sprite.position += p.vel * fElapsedTime;
            p.sprite.parms.x = min(0.5f*min(p.lifetimeStart-p.lifetime, 2.0f), 0.5f*min(p.lifetime, 2.0f)); // fade out... TODO
            p.sprite.parms.y = 1.0f;
        }
    }
    //
    // Update the vertex array
    //
    VertexSSprite *v;
    if(SUCCEEDED(hr = g_VertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0,(LPVOID*)&v)))
    {
        for(int i=0; i < NPARTICLES; i++)
        {
            v[i]  = g_particles[i].sprite;
        }
        g_VertexBuffer->Unmap();
        g_numParticles = NPARTICLES;
    } else {
        LOGMSG(LOG_WARN, L"Failed to Map vertex buffer\n");
    }
}
//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    if( g_D3DSettingsDlg.IsActive() )
    {
        ID3D10RenderTargetView *pRTView[2] = {DXUTGetD3D10RenderTargetView(), NULL}; // Due to a Bug, we must do this, right ?
        pd3dDevice->OMSetRenderTargets(2, pRTView, DXUTGetD3D10DepthStencilView());
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }
    if(!g_Initialised)
    {
        DXUTPause(true, true);
        return;
    }
    if(g_Animate || g_DebugTex)
        UpdateParticles(pd3dDevice, 0.025f);//fElapsedTime);

    if(g_Rotate)
    {
        D3DXVECTOR3 vFromPt   = D3DXVECTOR3(10.0f*cosf(g_Alpha), 2.3f, 6.0f*sinf(g_Alpha));
        D3DXVECTOR3 vLookatPt = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        g_camera.SetViewParams( &vFromPt, &vLookatPt);
        g_Alpha += 0.03f*3.14159f/180.0f;
    }

    D3DXMATRIX mProj = *g_camera.GetProjMatrix();
    D3DXMATRIX mView = *g_camera.GetViewMatrix();
    D3DXMATRIX mWorld = *g_camera.GetWorldMatrix();

    // Calculate the matrix world*view*proj
    D3DXMATRIX mViewI, mViewIT, mViewProj, mViewProjI;
    D3DXMATRIX mWorldI, mWorldIT;
    D3DXMATRIX mWorldView, mWorldViewI, mWorldViewIT;
    D3DXMATRIX mWorldViewProj, mWorldViewProjI;
    D3DXMatrixInverse(&mViewI, NULL, &mView);
    D3DXMatrixTranspose(&mViewIT, &mViewI);
    D3DXMatrixMultiply( &mViewProj, &mView, &mProj );
    D3DXMatrixInverse(&mViewProjI, NULL, &mViewProj);
    D3DXMatrixInverse(&mWorldI, NULL, &mWorld);
    D3DXMatrixTranspose(&mWorldIT, &mWorldI);
    D3DXMatrixMultiply( &mWorldView, &mWorld, &mView );
    D3DXMatrixInverse(&mWorldViewI, NULL, &mWorldView);
    D3DXMatrixTranspose(&mWorldViewIT, &mWorldViewI);
    D3DXMatrixMultiply( &mWorldViewProj, &mWorldView, &mProj );
    D3DXMatrixInverse(&mWorldViewProjI, NULL, &mWorldViewProj);

    g_pEffect->GetVariableByName("worldViewProj")->AsMatrix()->SetMatrix(mWorldViewProj);
    g_pEffect->GetVariableByName("viewProj")->AsMatrix()->SetMatrix(mViewProj);
    g_pEffect->GetVariableByName("worldView")->AsMatrix()->SetMatrix(mWorldView);
    g_pEffect->GetVariableByName("worldViewIT")->AsMatrix()->SetMatrix(mWorldViewIT);
    g_pEffect->GetVariableByName("proj")->AsMatrix()->SetMatrix(mProj);
    g_pEffect->GetVariableByName("worldIT")->AsMatrix()->SetMatrix(mWorldIT);
    g_pEffect->GetVariableByName("world")->AsMatrix()->SetMatrix(mWorld);
    g_pEffect->GetVariableByName("viewIT")->AsMatrix()->SetMatrix(mViewIT);
    g_pEffect->GetVariableByName("viewI")->AsMatrix()->SetMatrix(mViewI);
    g_pEffect->GetVariableByName("view")->AsMatrix()->SetMatrix(mView);
    //
    // Set the render targets
    // - RT for color
    // - RT for depth
    // Note: this is somehow bad to use the back buffer color here : AA mode can be a problem
    //
    switch(g_curTechnique)
    {
    case 0: // effect using ZBuffer as a texture
        RenderUsingZBufferAsTexture( pd3dDevice, (float)fTime, fElapsedTime, pUserContext, mWorldViewProj );
        break;
    case 1: // effect using an additional RT for Z
        RenderUsingFP32RTAsZBuffer( pd3dDevice, (float)fTime, fElapsedTime, pUserContext, mWorldViewProj, true );
        break;
    case 2: // Additional RT for Z and resolving MSAA in the shader
    case 3:
        RenderUsingFP32RTAsZBuffer( pd3dDevice, (float)fTime, fElapsedTime, pUserContext, mWorldViewProj, false );
        //
        // we don't want to use this technique in non MSAA modes : display a message and that's it
        //
        if(!g_pRTTexture2DDepth_res)
        {
            CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 15 );

            txtHelper.Begin();
            txtHelper.SetInsertionPos( 50, 300 );
            txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.0f, 0.4f, 1.0f ) );
            txtHelper.DrawTextLine( L"Not in MSAA mode. Technique not relevant" );
            txtHelper.End();
        }
        break;
    default:
    case 4: // reference case : no effect.
        RenderSimple( pd3dDevice, (float)fTime, fElapsedTime, pUserContext, mWorldViewProj );
        break;

    }
    //-------------------------------------------------------------------------------------
    //HUD
    //-------------------------------------------------------------------------------------
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    DXUT_EndPerfEvent();
    g_SampleUI.OnRender( fElapsedTime );
    g_HUD.OnRender( fElapsedTime );
    pd3dDevice->OMSetRenderTargets(0, NULL, NULL);
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice( void* pUserContext )
{
    g_D3DSettingsDlg.OnD3D10DestroyDevice();
    g_D3DSettingsDlg.GetDialogControl()->RemoveAllControls();
    g_DialogResourceManager.OnD3D10DestroyDevice();
    g_Mesh.Destroy();

    SAFE_RELEASE( g_pRTTexture2DDepth_res );
    SAFE_RELEASE( g_pTextureDepthView_res );
    SAFE_RELEASE( g_VertexBuffer );
    SAFE_RELEASE( g_pRTViewDepth );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pVertexLayoutSSprite );
    SAFE_RELEASE( g_pTextureDepthView );
    SAFE_RELEASE( g_pRTTexture2DDepth );
    SAFE_RELEASE( g_pDSTTexture    );
    SAFE_RELEASE( g_pDSTResView    );
    SAFE_RELEASE( g_pDSTView    );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pSprite );
    SAFE_RELEASE( g_pEffect );

    g_Skybox.OnDestroyDevice();
    SAFE_RELEASE(g_EnvMapSRV);
    SAFE_RELEASE(g_EnvMap);
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, 
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_camera.HandleMessages( hWnd, uMsg, wParam, lParam );
    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
  if(bKeyDown)
    switch(nChar)
    {
    case VK_OEM_PLUS:
    case VK_ADD:
        break;
    case VK_ESCAPE:
      PostQuitMessage( 0 );
        break;
    case ' ':
        g_Animate = g_Animate ? false : true;
        break;
    case '1':
        g_curTechnique = 0;
        break;
    case '2':
        g_curTechnique = 1;
        break;
    case '3':
        g_curTechnique = 2;
        break;
    }
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, 
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, 
                       int xPos, int yPos, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

   // Set the D3D10 DXUT callbacks. Remove these sets if the app doesn't need to support D3D10
    DXUTSetCallbackD3D10DeviceAcceptable( IsD3D10DeviceAcceptable );
    DXUTSetCallbackD3D10DeviceCreated( OnD3D10CreateDevice );
    DXUTSetCallbackD3D10SwapChainResized( OnD3D10ResizedSwapChain );
    DXUTSetCallbackD3D10FrameRender( OnD3D10FrameRender );
    DXUTSetCallbackD3D10SwapChainReleasing( OnD3D10ReleasingSwapChain );
    DXUTSetCallbackD3D10DeviceDestroyed( OnD3D10DestroyDevice );

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SoftParticles", NULL, NULL, NULL, 0, 0);

	// See ModifyDeviceSettings.  We set this flag on first creation to avoid the warning message box.
	g_InitialDeviceCreation = true;
    DXUTCreateDevice( true, 800, 600 );  
	g_InitialDeviceCreation = false;

    //
    // Camera
    //
    D3DXVECTOR3 vFromPt   = D3DXVECTOR3(10.0f*cosf(g_Alpha), 2.3f, 6.0f*sinf(g_Alpha));
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    g_camera.SetViewParams( &vFromPt, &vLookatPt);
    D3DXVECTOR3 vModelPt = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
    g_camera.SetModelCenter(vModelPt);

    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


