//
//  LevelGame.cpp
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/13/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#include "LevelGame.h"

Level *LevelGame::CurrentLevel() {
  return Levels[CurrentLevelIndex];
}

LevelGame::~LevelGame() {
  
}

void LevelGame::LoadNextLevel(Scene *scene) {
  CurrentLevel()->Load(scene);
}

void LevelGame::AddLevel(Level *level) {
  Levels.push_back(level);
}