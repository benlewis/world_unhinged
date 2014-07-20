//
//  Room.h
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/14/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#ifndef __osx_world_unhinged__Room__
#define __osx_world_unhinged__Room__

#include "LevelGame.h"
#include "tinyxml2.h"

#include <stdlib.h>
#include <string>

class Box;

class Room: public Level {
public:
  const static Vector3f FacetSize;

  Room(XMLElement *pXmlLevel);
  ~Room();
  
  virtual void Load(Scene *scene);
  virtual bool LevelComplete();
  virtual void Update();
  
protected:
  Box *GearBox                = nullptr;
  int StartingInventory   = 0;
  string Name;
  
};

#endif /* defined(__osx_world_unhinged__Room__) */
