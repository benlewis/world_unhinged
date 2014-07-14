//
//  LevelGame.h
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/13/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#ifndef __osx_world_unhinged__LevelGame__
#define __osx_world_unhinged__LevelGame__

#include <stdlib.h>
#include <vector>

#include "VRApp.h"

using namespace std;

class Level {
public:
  virtual void Load(Scene scene) = 0;
  virtual bool LevelComplete() = 0;
  
};

class LevelGame {
public:
  void LoadNextLevel(Scene scene);
  Level *CurrentLevel();
  virtual bool LoadLevels() = 0;
  
private:
  vector<Level *>   Levels;
  int               CurrentLevelIndex;

  
};

#endif /* defined(__osx_world_unhinged__LevelGame__) */
