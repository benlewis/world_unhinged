//
//  Box.cpp
//  osx_world_unhinged
//
//  Created by Ben Lewis on 7/14/14.
//  Copyright (c) 2014 Ben Lewis Personal. All rights reserved.
//

#include "Box.h"
#include "Room.h"

Box::Box(XMLElement* pXmlBox) {
  Size = Vector3f(pXmlBox->IntAttribute("width"),
                  pXmlBox->IntAttribute("height"),
                  pXmlBox->IntAttribute("length"));
  
  // Setup the pegs
  XMLElement* pXmlFace = pXmlBox->FirstChildElement("face");
  while (pXmlFace) {
    
    pXmlFace = pXmlFace->NextSiblingElement("face");
  }
  
  c = Color(188, 50, 50);
  Origin = Vector3f(0.0f, 1.75f, -1.5f);
  BuildModel();
  //AddBox(c, Origin, Vector3f(0.25f, 0.25f, 0.25f));
}

void Box::Update() {
  //Rotate(Quatf(0, 0, 1, 0.002));
  return;
  if (GetPosition().y > 2.5f)
    Move(Vector3f(0.0f, -2.5f, 0.0f));
  else
    Move(Vector3f(0.0f, 0.002f, 0.0f));

  
}

void Box::BuildModel()
{
  Vector3f a = Room::FacetSize;
  Vector3f s = Size * 0.5f * a;
  a = a * 0.5f;
  
  Vector3f o = Origin;
  UInt16   i = GetNextVertexIndex();
  
  // Back face
  AddVertex(-s.x + a.x + o.x,  s.y + o.y, -s.z + o.z,  c, 0, 1, 0, 0, -1);
  AddVertex(s.x - a.x  + o.x,  s.y + o.y, -s.z + o.z,  c, 1, 1, 0, 0, -1);
  AddVertex(s.x - a.x  + o.x, -s.y + o.y, -s.z + o.z,  c, 1, 0, 0, 0, -1);
  AddVertex(-s.x + a.x + o.x, -s.y + o.y, -s.z + o.z,  c, 0, 0, 0, 0, -1);
  AddTriangle(2 + i, 1 + i, 0 + i);
  AddTriangle(0 + i, 3 + i, 2 + i);
  
  
  // Front face
  AddVertex(s.x - a.x + o.x,  s.y + o.y,  s.z + o.z,  c, 1, 1, 0, 0, 1);
  AddVertex(-s.x + a.x + o.x,  s.y + o.y,  s.z + o.z,  c, 0, 1, 0, 0, 1);
  AddVertex(-s.x + a.x + o.x, -s.y + o.y,  s.z + o.z,  c, 0, 0, 0, 0, 1);
  AddVertex(s.x - a.x + o.x, -s.y + o.y,  s.z + o.z,  c, 1, 0, 0, 0, 1);
  AddTriangle(6 + i, 5 + i, 4 + i);
  AddTriangle(4 + i, 7 + i, 6 + i);
  
  // Left face
  AddVertex(-s.x + o.x,  s.y + o.y, -s.z + a.z + o.z,  c, 1, 0, -1, 0, 0);
  AddVertex(-s.x + o.x,  s.y + o.y,  s.z - a.z + o.z,  c, 1, 1, -1, 0, 0);
  AddVertex(-s.x + o.x, -s.y + o.y,  s.z - a.z + o.z,  c, 0, 1, -1, 0, 0);
  AddVertex(-s.x + o.x, -s.y + o.y, -s.z + a.z + o.z,  c, 0, 0, -1, 0, 0);
  AddTriangle(10 + i, 11 + i, 8  + i);
  AddTriangle(8  + i, 9  + i, 10 + i);
  
  // Right face
  AddVertex(s.x + o.x,  s.y + o.y, -s.z + a.z + o.z,  c, 1, 0, 1, 0, 0);
  AddVertex(s.x + o.x, -s.y + o.y, -s.z + a.z + o.z,  c, 0, 0, 1, 0, 0);
  AddVertex(s.x + o.x, -s.y + o.y,  s.z - a.z + o.z,  c, 0, 1, 1, 0, 0);
  AddVertex(s.x + o.x,  s.y + o.y,  s.z - a.z + o.z,  c, 1, 1, 1, 0, 0);
  AddTriangle(14 + i, 15 + i, 12 + i);
  AddTriangle(12 + i, 13 + i, 14 + i);
  
  
  // Bottom
  // Left Front Corner
  AddVertex(-s.x + o.x,       -s.y + o.y,  s.z - a.z + o.z,   c, 0, 1, 0, -1, 0);
  // Front Left Corner
  AddVertex(-s.x + a.x + o.x, -s.y + o.y,  s.z + o.z,         c, 0, 1, 0, -1, 0);
  
  // Front Right Corner
  AddVertex(s.x - a.x + o.x,  -s.y + o.y,  s.z + o.z,         c, 1, 1, 0, -1, 0);
  // Right Front Corner
  AddVertex(s.x + o.x,        -s.y + o.y,  s.z - a.z + o.z,   c, 1, 1, 0, -1, 0);
  
  // Right Back Corner
  AddVertex(s.x + o.x,        -s.y + o.y, -s.z + a.z + o.z,   c, 1, 0, 0, -1, 0);
  // Back Right Corner
  AddVertex(s.x - a.x + o.x,  -s.y + o.y, -s.z + o.z,         c, 1, 0, 0, -1, 0);
  
  // Back Left Corner
  AddVertex(-s.x + a.x + o.x, -s.y + o.y, -s.z + o.z,         c, 0, 0, 0, -1, 0);
  // Left Back Corner
  AddVertex(-s.x+ o.x,        -s.y + o.y, -s.z + a.z + o.z,   c, 0, 0, 0, -1, 0);
  
  AddTriangle(16 + i, 17 + i, 18 + i);
  AddTriangle(18 + i, 19 + i, 20 + i);
  AddTriangle(20 + i, 21 + i, 22 + i);
  AddTriangle(22 + i, 23 + i, 16 + i);
  AddTriangle(16 + i, 18 + i, 22 + i);
  AddTriangle(22 + i, 18 + i, 20 + i);

  
  // Top
  // Left Front Corner
  AddVertex(-s.x + o.x,       s.y + o.y,  s.z - a.z + o.z,    c, 0, 0, 0, 1, 0);
  // Front Left Corner
  AddVertex(-s.x + a.x + o.x, s.y + o.y,  s.z + o.z,          c, 0, 0, 0, 1, 0);
  
  // Front Right Corner
  AddVertex(s.x - a.x + o.x,  s.y + o.y,  s.z + o.z,          c, 1, 0, 0, 1, 0);
  // Right Front Corner
  AddVertex(s.x + o.x,        s.y + o.y,  s.z - a.z + o.z,    c, 1, 0, 0, 1, 0);
  
  // Right Back Corner
  AddVertex(s.x + o.x,        s.y + o.y, -s.z + a.z + o.z,    c, 1, 1, 0, 1, 0);
  // Back Right Corner
  AddVertex(s.x - a.x + o.x,  s.y + o.y, -s.z + o.z,          c, 1, 1, 0, 1, 0);
  
  // Back Left Corner
  AddVertex(-s.x + a.x + o.x, s.y + o.y, -s.z + o.z,          c, 0, 1, 0, 1, 0);
  // Left Back Corner
  AddVertex(-s.x + o.x,       s.y + o.y, -s.z + a.z + o.z,    c, 0, 1, 0, 1, 0);
  
  AddTriangle(24 + i, 26 + i, 25 + i);
  AddTriangle(26 + i, 28 + i, 27 + i);
  AddTriangle(28 + i, 30 + i, 29 + i);
  AddTriangle(30 + i, 24 + i, 31 + i);
  AddTriangle(28 + i, 26 + i, 24 + i);
  AddTriangle(28 + i, 24 + i, 30 + i);
  
  // Front Left Corner
  AddTriangle(24 + i, 25 + i, 16 + i);
  AddTriangle(16 + i, 25 + i, 17 + i);
  
  // Front Right Corner
  AddTriangle(26 + i, 27 + i, 18 + i);
  AddTriangle(18 + i, 27 + i, 19 + i);
  
  // Back Left Corner
  AddTriangle(30 + i, 31 + i, 22 + i);
  AddTriangle(22 + i, 31 + i, 23 + i);
  
  // Back Right Corner
  AddTriangle(28 + i, 29 + i, 20 + i);
  AddTriangle(20 + i, 29 + i, 21 + i);
}
