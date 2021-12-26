/**
 * \file unitcomm_Tests.cpp
 *
 * \brief Unit-tests for string utilities
 *
 * \author vivek
 * Proprietary and Confidential Material.
 */
#include <iostream>
#include <vector>
//#include <ifstream>
#include <iomanip>
#include <fstream>
#include <string>
#include "unitcomm.h"

#include <gtest/gtest.h>

using namespace std;

int value=0;

TEST(unitcomm, wanUp)
{

	bool ret=wanUp();
	EXPECT_EQ(true, ret);
}

TEST(unitcomm, getdefaultRoute)
{
	bool route=getdefaultRoute();
	EXPECT_EQ(true, route);
}

TEST(unitcomm, findConnection) {
 


int value = findConnection(string ip, int port); 
    
    if (value != -1)
    {
          cout << "find the connection";
    }
  

    EXPECT_EQ(true , value); //location is fixed if antennas are attached
}
TEST(unitcomm, getSwVersion)
{
        bool success=false;


        string InputString="C58DB00014";
  
	//std::string InputString = "";
        
       
	EXPECT_EQ(true, success);
         
}

