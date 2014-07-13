//
//  VRScene.cpp
//  osx_world_vr_scene
//
//  Created by Ben Lewis on 7/11/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

/************************************************************************************
 
 Filename    :   VRScene.cpp
 Content     :   Logic for loading, and creating rendered scene components,
 cube and grid overlays, etc.
 Created     :   October 4, 2012
 Authors     :   Michael Antonov, Andrew Reisse, Steve LaValle, Dov Katz
 Peter Hoff, Dan Goodman, Bryan Croteau
 
 Copyright   :   Copyright 2012 Oculus VR, Inc. All Rights reserved.
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 
 *************************************************************************************/

#include "VRApp.h"



Fill* CreateTexureFill(RenderDevice* prender, const String& filename)
{
  Ptr<File>    imageFile = *new SysFile(filename);
  Ptr<Texture> imageTex;
  if (imageFile->IsValid())
    imageTex = *LoadTextureTga(prender, imageFile);
  
  // Image is rendered as a single quad.
  ShaderFill* fill = 0;
  if (imageTex)
  {
    imageTex->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
    fill = new ShaderFill(*prender->CreateShaderSet());
    fill->GetShaders()->SetShader(prender->LoadBuiltinShader(Shader_Vertex, VShader_MVP));
    fill->GetShaders()->SetShader(prender->LoadBuiltinShader(Shader_Fragment, FShader_Texture));
    fill->SetTexture(0, imageTex);
  }
  
  return fill;
}


// Creates a grid of cubes.
void PopulateCubeFieldScene(Scene* scene, Fill* fill,
                            int cubeCountX, int cubeCountY, int cubeCountZ, Vector3f offset,
                            float cubeSpacing = 0.5f, float cubeSize = 0.1f)
{
  
  Vector3f corner(-(((cubeCountX-1) * cubeSpacing) + cubeSize) * 0.5f,
                  -(((cubeCountY-1) * cubeSpacing) + cubeSize) * 0.5f,
                  -(((cubeCountZ-1) * cubeSpacing) + cubeSize) * 0.5f);
  corner += offset;
  
  Vector3f pos = corner;
  
  for (int i = 0; i < cubeCountX; i++)
  {
    // Create a new model for each 'plane' of cubes so we don't exceed
    // the vert size limit.
    Ptr<Model> model = *new Model();
    scene->World.Add(model);
    
    if (fill)
      model->Fill = fill;
    
    for (int j = 0; j < cubeCountY; j++)
    {
      for (int k = 0; k < cubeCountZ; k++)
      {
        model->AddBox(0xFFFFFFFF, pos, Vector3f(cubeSize, cubeSize, cubeSize));
        pos.z += cubeSpacing;
      }
      
      pos.z = corner.z;
      pos.y += cubeSpacing;
    }
    
    pos.y = corner.y;
    pos.x += cubeSpacing;
  }
}



