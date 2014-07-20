//
//  Room.cpp
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/14/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#include "Room.h"
#include "Box.h"

const Vector3f Room::FacetSize = Vector3f(0.25f, 0.25f, 0.25f);

Room::Room(XMLElement* pXmlRoom) {
  Name = pXmlRoom->Attribute("name");
  
  XMLElement* pXmlBox = pXmlRoom->FirstChildElement("box");
  if (pXmlBox)
  GearBox = new Box(pXmlBox);
  
  XMLElement* pXmlInventory = pXmlRoom->FirstChildElement("inventory");
  if (pXmlInventory)
    StartingInventory = pXmlInventory->FirstChildElement("gears")->IntAttribute("count");
  
}

Room::~Room() {
  if (GearBox)
  delete(GearBox);
}

void Room::Update() {
  if (GearBox == nullptr)
    return;
  
  GearBox->Update();
}

void Room::Load(Scene *scene) {
  scene->World.Add(GearBox);
  scene->Models.PushBack(GearBox);
  scene->SetAmbient(Color4f(1.0f, 1.0f, 1.0f, 1.0f));
}

bool Room::LevelComplete() {
  
  return false;
}