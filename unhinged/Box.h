//
//  Box.h
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/14/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#ifndef __osx_world_unhinged__Box__
#define __osx_world_unhinged__Box__

#include "LevelGame.h"
#include "tinyxml2.h"

class Box: public Model {
  // Holds Facets/Gears
  // Might not actually be a box shape
public:
  Box(XMLElement *pXmlBox);
  void BuildModel();
  void Update();
  
protected:
  Vector3f Size, Origin;
  Color c;
};


#endif /* defined(__osx_world_unhinged__Box__) */
