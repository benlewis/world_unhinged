//
//  UnhingedGame.cpp
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/12/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#include "UnhingedGame.h"
#include "Room.h"
#include "Box.h"

#include "tinyxml2.h"

#include <stdlib.h>
#include <string>

using namespace std;

LevelGame *CreateGameObject() {
  return new UnhingedGame();
}

UnhingedGame::UnhingedGame() {
  LoadLevels();
}

UnhingedGame::~UnhingedGame() {
  
}

bool UnhingedGame::LoadLevels() {
  // Load from game_data.xml
  tinyxml2::XMLDocument* pXmlDocument = new tinyxml2::XMLDocument();;
 
  if(pXmlDocument->LoadFile("/Users/benlewis/src/world_unhinged/Assets/game_data.xml") != 0)
  {
    return false;
  }
  
  XMLElement* pXmlLevels= pXmlDocument->FirstChildElement("levels");
  
  if (pXmlLevels) {
    XMLElement* pXmlLevel = pXmlLevels->FirstChildElement("level");
    while (pXmlLevel) {
      Room* room = new Room(pXmlLevel);
      AddLevel(room);
      pXmlLevel = pXmlLevel->NextSiblingElement("level");
    }
  }
  
  CurrentLevelIndex = 0;
  
  return true;
}

