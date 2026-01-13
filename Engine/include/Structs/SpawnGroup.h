#include <iostream>

struct SpawnGroupResolved
{
    std::string id;
    std::string unitType;
    int count = 0;
    float originX = 0.0f;
    float originZ = 0.0f;
    float jitterM = 0.0f;
    std::string formationKind;
    int columns = 0;
    float circleRadiusM = 0.0f;
    bool spacingAuto = true;
    float spacingM = 0.0f;
};