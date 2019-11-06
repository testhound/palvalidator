#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <iostream>
#include "PalAst.h"
#include "PalParseDriver.h"
#include "PalCodeGenVisitor.h"
#include <exception>
#include "StopTargetDetail.h"

extern std::string GetBaseFilename(const char *filename);

//void
//printStopTargetDetail(const StopTargetDetailReader& reader)
//{
//  StopTargetDetail dev1 (reader.getDev1Detail());
//  StopTargetDetail dev2 (reader.getDev2Detail());

//  std::cout << "Dev1 detail:" << std::endl;
//  std::cout << "Dev1 stop, target = " << dev1.getStopLoss() << std::string (", ") <<dev1.getProfitTarget() << std::endl;
//  std::cout << "Dev1 minhold, maxhold = " << dev1.getMinHoldingPeriod() << std::string (", ") << dev1.getMaxHoldingPeriod() << std::endl << std::endl;

//  std::cout << "Dev2 detail:" << std::endl;
//  std::cout << "Dev2 stop, target = " << dev2.getStopLoss() << std::string (", ") << dev2.getProfitTarget() << std::endl;
//  std::cout << "Dev2 minhold, maxhold = " << dev2.getMinHoldingPeriod() << std::string (", ") << dev2.getMaxHoldingPeriod() << std::endl;

//}

int main(int argc, char **argv)
{
  try
    {
      std::vector<std::string> v(argv, argv + argc);

      // v[0] = executable name
      // v[1] = PAL IR file

      if (argc == 3)
        {
          printf ("Argument count = %d\n", argc);
          printf("Argument 1 = %s\n", v[1].c_str());
          printf("Argument 2 = %s\n", v[2].c_str());
          //printf ("Argument 3 = %s\n", v[3].c_str());

          mkc_palast::PalParseDriver driver (v[1]);

//          StopTargetDetailReader detailReader (v[3]);

//          printStopTargetDetail (detailReader);

	  std::string easyLanguageFileName = GetBaseFilename(v[1].c_str()) + std::string("_eld") + std::string (".txt");

	  driver.Parse();
	  PriceActionLabSystem* system = driver.getPalStrategies();

	  std::cout << "Generating EasyLanguage code" << std::endl;
	  EasyLanguagePointAdjustedCodeGenVisitor codeGen(system,
							  v[2],
							  easyLanguageFileName
//							  detailReader.getDev1Detail(),
//							  detailReader.getDev2Detail()
	      );

	  codeGen.generateCode();
	}
      else
        {
          printf ("Usage: arg1 = PAL soundIR File; arg2 = EL template File\n");
        }
    }
  catch (std::exception& e)
    {
      std::cout << e.what() << std::endl;
    }
  return 0;
}

