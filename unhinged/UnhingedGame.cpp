//
//  UnhingedGame.cpp
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/12/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#include "UnhingedGame.h"

#include "tinyxml2.h"

#include <stdlib.h>
#include <string>

using namespace std;

LevelGame *CreateGameObject() {
  return new UnhingedGame();
}

UnhingedGame::UnhingedGame() {
  
}

UnhingedGame::~UnhingedGame() {
  
}

bool UnhingedGame::LoadLevels() {
  // Load from game_data.xml
  tinyxml2::XMLDocument* pXmlDocument = new tinyxml2::XMLDocument();;
 
  if(pXmlDocument->LoadFile("game_data.xml") != 0)
  {
    return false;
  }
  
  XMLElement* pXmlTexture = pXmlDocument->FirstChildElement("levels");
  
  return true;
}

