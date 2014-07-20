//
//  UnhingedGame.h
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/12/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#ifndef __osx_world_unhinged__UnhingedGame__
#define __osx_world_unhinged__UnhingedGame__

#include "LevelGame.h"

#include <stdlib.h>
#include <string>

class UnhingedGame: public LevelGame {
public:
  UnhingedGame();
  ~UnhingedGame();
  
  virtual bool LoadLevels();
};




#endif /* defined(__osx_world_unhinged__UnhingedGame__) */
