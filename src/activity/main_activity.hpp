#pragma once

#include <borealis.hpp>

// Root activity: the sidebar with the five main tabs.
class MainActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/main.xml");
};
