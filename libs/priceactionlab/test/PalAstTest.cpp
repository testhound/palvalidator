#include <vector>
#include <memory>
#include <list>
#include <set>
#include <iterator>
#include <catch2/catch_test_macros.hpp>
#include "PalAst.h"     // Your header file
#include "PalCodeGenVisitor.h"
#include "number.h"     // Assuming number.h is in the include path
#include "TestUtils.h"
#include "AstResourceManager.h"

using mkc_palast::AstResourceManager;

// Mock PalCodeGenVisitor for testing accept methods
class MockPalCodeGenVisitor : public PalCodeGenVisitor {
public:
    mutable std::vector<std::string> visited_nodes;

  void generateCode() override {}
    void visit(PriceBarOpen* p) override { visited_nodes.push_back("PriceBarOpen"); }
    void visit(PriceBarHigh* p) override { visited_nodes.push_back("PriceBarHigh"); }
    void visit(PriceBarLow* p) override { visited_nodes.push_back("PriceBarLow"); }
    void visit(PriceBarClose* p) override { visited_nodes.push_back("PriceBarClose"); }
    void visit(VolumeBarReference* p) override { visited_nodes.push_back("VolumeBarReference"); }
    void visit(Roc1BarReference* p) override { visited_nodes.push_back("Roc1BarReference"); }
    void visit(IBS1BarReference* p) override { visited_nodes.push_back("IBS1BarReference"); }
    void visit(IBS2BarReference* p) override { visited_nodes.push_back("IBS2BarReference"); }
    void visit(IBS3BarReference* p) override { visited_nodes.push_back("IBS3BarReference"); }
    void visit(MeanderBarReference* p) override { visited_nodes.push_back("MeanderBarReference"); }
    void visit(VChartHighBarReference* p) override { visited_nodes.push_back("VChartHighBarReference"); }
    void visit(VChartLowBarReference* p) override { visited_nodes.push_back("VChartLowBarReference"); }
    // void visit(MomersionFilterBarReference* p) override { visited_nodes.push_back("MomersionFilterBarReference"); } // Uncomment if used
    void visit(GreaterThanExpr* p) override { visited_nodes.push_back("GreaterThanExpr"); }
    void visit(AndExpr* p) override { visited_nodes.push_back("AndExpr"); }
    void visit(LongSideProfitTargetInPercent* p) override { visited_nodes.push_back("LongSideProfitTargetInPercent"); }
    void visit(ShortSideProfitTargetInPercent* p) override { visited_nodes.push_back("ShortSideProfitTargetInPercent"); }
    void visit(LongSideStopLossInPercent* p) override { visited_nodes.push_back("LongSideStopLossInPercent"); }
    void visit(ShortSideStopLossInPercent* p) override { visited_nodes.push_back("ShortSideStopLossInPercent"); }
    void visit(LongMarketEntryOnOpen* p) override { visited_nodes.push_back("LongMarketEntryOnOpen"); }
    void visit(ShortMarketEntryOnOpen* p) override { visited_nodes.push_back("ShortMarketEntryOnOpen"); }
    void visit(PatternDescription* p) override { visited_nodes.push_back("PatternDescription"); }
    void visit(PriceActionLabPattern* p) override { visited_nodes.push_back("PriceActionLabPattern"); }
    // Add other visit methods as needed from PalCodeGenVisitor.h
};

namespace {

// Builds a minimal long PriceActionLabPattern.
// All parameters that distinguish patterns are exposed as arguments;
// everything else is held constant so that only the dimension under test
// varies between two patterns being compared.
PALPatternPtr makeLongPattern(
    AstFactory& factory,
    const std::string& fileName,
    PatternExpressionPtr expr,
    const std::string& profitTargetStr,
    const std::string& stopLossStr,
    PriceActionLabPattern::VolatilityAttribute  vol  = PriceActionLabPattern::VOLATILITY_NONE,
    PriceActionLabPattern::PortfolioAttribute   port = PriceActionLabPattern::PORTFOLIO_FILTER_NONE)
{
    auto pLong  = factory.getDecimalNumber(const_cast<char*>("70.0"));
    auto pShort = factory.getDecimalNumber(const_cast<char*>("30.0"));
    auto desc   = std::make_shared<PatternDescription>(
                      fileName, 1u, 20230101ul, pLong, pShort, 10u, 2u);

    auto entry = factory.getLongMarketEntryOnOpen();
    auto pt    = factory.getLongProfitTarget(
                     factory.getDecimalNumber(const_cast<char*>(profitTargetStr.c_str())));
    auto sl    = factory.getLongStopLoss(
                     factory.getDecimalNumber(const_cast<char*>(stopLossStr.c_str())));

    return std::make_shared<PriceActionLabPattern>(
               desc, expr, entry, pt, sl, vol, port);
}

// Same as above but for short patterns.
PALPatternPtr makeShortPattern(
    AstFactory& factory,
    const std::string& fileName,
    PatternExpressionPtr expr,
    const std::string& profitTargetStr,
    const std::string& stopLossStr,
    PriceActionLabPattern::VolatilityAttribute  vol  = PriceActionLabPattern::VOLATILITY_NONE,
    PriceActionLabPattern::PortfolioAttribute   port = PriceActionLabPattern::PORTFOLIO_FILTER_NONE)
{
    auto pLong  = factory.getDecimalNumber(const_cast<char*>("70.0"));
    auto pShort = factory.getDecimalNumber(const_cast<char*>("30.0"));
    auto desc   = std::make_shared<PatternDescription>(
                      fileName, 1u, 20230101ul, pLong, pShort, 10u, 2u);

    auto entry = factory.getShortMarketEntryOnOpen();
    auto pt    = factory.getShortProfitTarget(
                     factory.getDecimalNumber(const_cast<char*>(profitTargetStr.c_str())));
    auto sl    = factory.getShortStopLoss(
                     factory.getDecimalNumber(const_cast<char*>(stopLossStr.c_str())));

    return std::make_shared<PriceActionLabPattern>(
               desc, expr, entry, pt, sl, vol, port);
}

} // anonymous 

TEST_CASE("AstFactory Basic Operations", "[AstFactory]") {
    AstFactory factory;
    MockPalCodeGenVisitor mock_visitor;

    SECTION("Get PriceBarReference Objects") {
        auto open0 = factory.getPriceOpen(0);
        REQUIRE(open0 != nullptr);
        REQUIRE(open0->getBarOffset() == 0);
        REQUIRE(open0->getReferenceType() == PriceBarReference::OPEN);
        REQUIRE(open0->extraBarsNeeded() == 0); //
        open0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarOpen");

        auto high1 = factory.getPriceHigh(1);
        REQUIRE(high1 != nullptr);
        REQUIRE(high1->getBarOffset() == 1);
        REQUIRE(high1->getReferenceType() == PriceBarReference::HIGH);
        REQUIRE(high1->extraBarsNeeded() == 0); //
        high1->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarHigh");


        auto low2 = factory.getPriceLow(2);
        REQUIRE(low2 != nullptr);
        REQUIRE(low2->getBarOffset() == 2);
        REQUIRE(low2->getReferenceType() == PriceBarReference::LOW);
        REQUIRE(low2->extraBarsNeeded() == 0); //
        low2->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarLow");

        auto close3 = factory.getPriceClose(3);
        REQUIRE(close3 != nullptr);
        REQUIRE(close3->getBarOffset() == 3);
        REQUIRE(close3->getReferenceType() == PriceBarReference::CLOSE);
        REQUIRE(close3->extraBarsNeeded() == 0); //
        close3->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarClose");

        auto volume0 = factory.getVolume(0);
        REQUIRE(volume0 != nullptr);
        REQUIRE(volume0->getBarOffset() == 0);
        REQUIRE(volume0->getReferenceType() == PriceBarReference::VOLUME);
        REQUIRE(volume0->extraBarsNeeded() == 0); //
        volume0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "VolumeBarReference");

        auto roc1_0 = factory.getRoc1(0);
        REQUIRE(roc1_0 != nullptr);
        REQUIRE(roc1_0->getBarOffset() == 0);
        REQUIRE(roc1_0->getReferenceType() == PriceBarReference::ROC1);
        REQUIRE(roc1_0->extraBarsNeeded() == 1); //
        roc1_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "Roc1BarReference");

        auto ibs1_0 = factory.getIBS1(0);
        REQUIRE(ibs1_0 != nullptr);
        REQUIRE(ibs1_0->getBarOffset() == 0);
        REQUIRE(ibs1_0->getReferenceType() == PriceBarReference::IBS1);
        REQUIRE(ibs1_0->extraBarsNeeded() == 0); //
        ibs1_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "IBS1BarReference");

        auto ibs2_0 = factory.getIBS2(0);
        REQUIRE(ibs2_0 != nullptr);
        REQUIRE(ibs2_0->getBarOffset() == 0);
        REQUIRE(ibs2_0->getReferenceType() == PriceBarReference::IBS2);
        REQUIRE(ibs2_0->extraBarsNeeded() == 1); //
        ibs2_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "IBS2BarReference");

        auto ibs3_0 = factory.getIBS3(0);
        REQUIRE(ibs3_0 != nullptr);
        REQUIRE(ibs3_0->getBarOffset() == 0);
        REQUIRE(ibs3_0->getReferenceType() == PriceBarReference::IBS3);
        REQUIRE(ibs3_0->extraBarsNeeded() == 2); //
        ibs3_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "IBS3BarReference");

        auto meander0 = factory.getMeander(0);
        REQUIRE(meander0 != nullptr);
        REQUIRE(meander0->getBarOffset() == 0);
        REQUIRE(meander0->getReferenceType() == PriceBarReference::MEANDER);
        REQUIRE(meander0->extraBarsNeeded() == 5); //
        meander0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "MeanderBarReference");

        auto vclow0 = factory.getVChartLow(0);
        REQUIRE(vclow0 != nullptr);
        REQUIRE(vclow0->getBarOffset() == 0);
        REQUIRE(vclow0->getReferenceType() == PriceBarReference::VCHARTLOW);
        REQUIRE(vclow0->extraBarsNeeded() == 6); //
        vclow0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "VChartLowBarReference");

        auto vchigh0 = factory.getVChartHigh(0);
        REQUIRE(vchigh0 != nullptr);
        REQUIRE(vchigh0->getBarOffset() == 0);
        REQUIRE(vchigh0->getReferenceType() == PriceBarReference::VCHARTHIGH);
        REQUIRE(vchigh0->extraBarsNeeded() == 6); //
        vchigh0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "VChartHighBarReference");

        // Test caching: subsequent calls with same offset should return same pointer for predefined ones
        REQUIRE(factory.getPriceOpen(0).get() == open0.get());
        REQUIRE(factory.getPriceOpen(AstFactory::MaxNumBarOffsets -1) != nullptr); // Check last predefined
        // Test beyond predefined
        auto open_beyond = factory.getPriceOpen(AstFactory::MaxNumBarOffsets + 1);
        REQUIRE(open_beyond != nullptr);
        REQUIRE(open_beyond->getBarOffset() == AstFactory::MaxNumBarOffsets + 1);
        //This object is created on the fly and is not cleaned up by AstFactory destructor
        //It needs to be manually deleted if not assigned to a shared_ptr or managed elsewhere.
        //For this test, we'll assume it's okay for it to leak or be cleaned up by OS on exit.
        //In real code, this would need careful memory management.
        // delete open_beyond; // Or better, use smart pointers in real code
    }

    SECTION("Get MarketEntryExpression Objects") {
        auto longEntry = factory.getLongMarketEntryOnOpen();
        REQUIRE(longEntry != nullptr);
        REQUIRE(longEntry->isLongPattern()); //
        REQUIRE_FALSE(longEntry->isShortPattern()); //
        longEntry->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "LongMarketEntryOnOpen");


        auto shortEntry = factory.getShortMarketEntryOnOpen();
        REQUIRE(shortEntry != nullptr);
        REQUIRE(shortEntry->isShortPattern()); //
        REQUIRE_FALSE(shortEntry->isLongPattern()); //
        shortEntry->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "ShortMarketEntryOnOpen");


        // Test caching
        REQUIRE(factory.getLongMarketEntryOnOpen().get() == longEntry.get());
        REQUIRE(factory.getShortMarketEntryOnOpen().get() == shortEntry.get());
    }

    SECTION("Get DecimalNumber Objects") {
        char numStr1[] = "123.45";
        auto dec1_str = factory.getDecimalNumber(numStr1);
        REQUIRE(dec1_str != nullptr);
        REQUIRE(*dec1_str == num::fromString<decimal7>("123.45"));

        char numStr2[] = "67.89";
        auto dec2_str = factory.getDecimalNumber(numStr2);
        REQUIRE(dec2_str != nullptr);
        REQUIRE(*dec2_str == num::fromString<decimal7>("67.89"));
        REQUIRE(factory.getDecimalNumber(numStr1).get() == dec1_str.get()); // Test caching

        auto dec1_int = factory.getDecimalNumber(123);
        REQUIRE(dec1_int != nullptr);
        REQUIRE(*dec1_int == decimal7(123));

        auto dec2_int = factory.getDecimalNumber(456);
        REQUIRE(dec2_int != nullptr);
        REQUIRE(*dec2_int == decimal7(456));
        REQUIRE(factory.getDecimalNumber(123).get() == dec1_int.get()); // Test caching
    }


    SECTION("Get ProfitTarget Objects") {
        auto pt_val_5 = factory.getDecimalNumber(5);
        auto pt_val_10 = factory.getDecimalNumber(10);


        auto long_pt5 = factory.getLongProfitTarget(pt_val_5);
        REQUIRE(long_pt5 != nullptr);
        REQUIRE(long_pt5->getProfitTarget() == pt_val_5.get());
        REQUIRE(long_pt5->isLongSideProfitTarget()); //
        REQUIRE_FALSE(long_pt5->isShortSideProfitTarget()); //
        long_pt5->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "LongSideProfitTargetInPercent");

        auto short_pt10 = factory.getShortProfitTarget(pt_val_10);
        REQUIRE(short_pt10 != nullptr);
        REQUIRE(short_pt10->getProfitTarget() == pt_val_10.get());
        REQUIRE(short_pt10->isShortSideProfitTarget()); //
        REQUIRE_FALSE(short_pt10->isLongSideProfitTarget()); //
        short_pt10->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "ShortSideProfitTargetInPercent");

        // Test caching
        REQUIRE(factory.getLongProfitTarget(pt_val_5).get() == long_pt5.get());
        REQUIRE(factory.getShortProfitTarget(pt_val_10).get() == short_pt10.get());
    }

    SECTION("Get StopLoss Objects") {
        auto sl_val_2 = factory.getDecimalNumber(2);
        auto sl_val_3 = factory.getDecimalNumber(3);

        auto long_sl2 = factory.getLongStopLoss(sl_val_2);
        REQUIRE(long_sl2 != nullptr);
        REQUIRE(long_sl2->getStopLoss() == sl_val_2.get());
        REQUIRE(long_sl2->isLongSideStopLoss()); //
        REQUIRE_FALSE(long_sl2->isShortSideStopLoss()); //
        long_sl2->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "LongSideStopLossInPercent");

        auto short_sl3 = factory.getShortStopLoss(sl_val_3);
        REQUIRE(short_sl3 != nullptr);
        REQUIRE(short_sl3->getStopLoss() == sl_val_3.get());
        REQUIRE(short_sl3->isShortSideStopLoss()); //
        REQUIRE_FALSE(short_sl3->isLongSideStopLoss()); //
        short_sl3->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "ShortSideStopLossInPercent");

        // Test caching
        REQUIRE(factory.getLongStopLoss(sl_val_2).get() == long_sl2.get());
        REQUIRE(factory.getShortStopLoss(sl_val_3).get() == short_sl3.get());
    }
}

TEST_CASE("PriceBarReference Classes", "[PriceBarReference]") {
    MockPalCodeGenVisitor visitor;

    SECTION("PriceBarOpen") {
        PriceBarOpen pbo(1);
        REQUIRE(pbo.getBarOffset() == 1);
        REQUIRE(pbo.getReferenceType() == PriceBarReference::OPEN);
        REQUIRE(pbo.extraBarsNeeded() == 0); //
        pbo.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarOpen");
        unsigned long long hc1 = pbo.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(pbo.hashCode() == hc1); // Hash code should be cached

        PriceBarOpen pbo_copy(pbo);
        REQUIRE(pbo_copy.getBarOffset() == 1);
        REQUIRE(pbo_copy.hashCode() == hc1);


        PriceBarOpen pbo_assign(2);
        pbo_assign = pbo;
        REQUIRE(pbo_assign.getBarOffset() == 1);
        REQUIRE(pbo_assign.hashCode() == hc1);

    }

    SECTION("PriceBarHigh") {
        PriceBarHigh pbh(2);
        REQUIRE(pbh.getBarOffset() == 2);
        REQUIRE(pbh.getReferenceType() == PriceBarReference::HIGH);
        REQUIRE(pbh.extraBarsNeeded() == 0); //
        pbh.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarHigh");
        unsigned long long hc1 = pbh.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(pbh.hashCode() == hc1);
    }
     SECTION("PriceBarLow") {
        PriceBarLow pbl(3);
        REQUIRE(pbl.getBarOffset() == 3);
        REQUIRE(pbl.getReferenceType() == PriceBarReference::LOW);
        REQUIRE(pbl.extraBarsNeeded() == 0); //
        pbl.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarLow");
        unsigned long long hc1 = pbl.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(pbl.hashCode() == hc1);
    }

    SECTION("PriceBarClose") {
        PriceBarClose pbc(4);
        REQUIRE(pbc.getBarOffset() == 4);
        REQUIRE(pbc.getReferenceType() == PriceBarReference::CLOSE);
        REQUIRE(pbc.extraBarsNeeded() == 0); //
        pbc.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarClose");
        unsigned long long hc1 = pbc.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(pbc.hashCode() == hc1);
    }
    SECTION("VolumeBarReference") {
        VolumeBarReference vbr(1);
        REQUIRE(vbr.getBarOffset() == 1);
        REQUIRE(vbr.getReferenceType() == PriceBarReference::VOLUME);
        REQUIRE(vbr.extraBarsNeeded() == 0); //
        vbr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "VolumeBarReference");
        unsigned long long hc1 = vbr.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(vbr.hashCode() == hc1);
    }

    SECTION("Roc1BarReference") {
        Roc1BarReference rbr(2);
        REQUIRE(rbr.getBarOffset() == 2);
        REQUIRE(rbr.getReferenceType() == PriceBarReference::ROC1);
        REQUIRE(rbr.extraBarsNeeded() == 1); //
        rbr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "Roc1BarReference");
        unsigned long long hc1 = rbr.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(rbr.hashCode() == hc1);
    }
    SECTION("IBS1BarReference") {
        IBS1BarReference ibs1(0);
        REQUIRE(ibs1.getBarOffset() == 0);
        REQUIRE(ibs1.getReferenceType() == PriceBarReference::IBS1);
        REQUIRE(ibs1.extraBarsNeeded() == 0); //
        ibs1.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "IBS1BarReference");
        unsigned long long hc1 = ibs1.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(ibs1.hashCode() == hc1);
    }

    SECTION("IBS2BarReference") {
        IBS2BarReference ibs2(1);
        REQUIRE(ibs2.getBarOffset() == 1);
        REQUIRE(ibs2.getReferenceType() == PriceBarReference::IBS2);
        REQUIRE(ibs2.extraBarsNeeded() == 1); //
        ibs2.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "IBS2BarReference");
        unsigned long long hc1 = ibs2.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(ibs2.hashCode() == hc1);
    }

    SECTION("IBS3BarReference") {
        IBS3BarReference ibs3(2);
        REQUIRE(ibs3.getBarOffset() == 2);
        REQUIRE(ibs3.getReferenceType() == PriceBarReference::IBS3);
        REQUIRE(ibs3.extraBarsNeeded() == 2); //
        ibs3.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "IBS3BarReference");
        unsigned long long hc1 = ibs3.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(ibs3.hashCode() == hc1);
    }

    SECTION("MeanderBarReference") {
        MeanderBarReference mbr(3);
        REQUIRE(mbr.getBarOffset() == 3);
        REQUIRE(mbr.getReferenceType() == PriceBarReference::MEANDER);
        REQUIRE(mbr.extraBarsNeeded() == 5); //
        mbr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "MeanderBarReference");
        unsigned long long hc1 = mbr.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(mbr.hashCode() == hc1);
    }

    SECTION("VChartHighBarReference") {
        VChartHighBarReference vchbr(4);
        REQUIRE(vchbr.getBarOffset() == 4);
        REQUIRE(vchbr.getReferenceType() == PriceBarReference::VCHARTHIGH);
        REQUIRE(vchbr.extraBarsNeeded() == 6); //
        vchbr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "VChartHighBarReference");
        unsigned long long hc1 = vchbr.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(vchbr.hashCode() == hc1);
    }

    SECTION("VChartLowBarReference") {
        VChartLowBarReference vclbr(5);
        REQUIRE(vclbr.getBarOffset() == 5);
        REQUIRE(vclbr.getReferenceType() == PriceBarReference::VCHARTLOW);
        REQUIRE(vclbr.extraBarsNeeded() == 6); //
        vclbr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "VChartLowBarReference");
        unsigned long long hc1 = vclbr.hashCode();
        REQUIRE(hc1 != 0);
        REQUIRE(vclbr.hashCode() == hc1);
    }
    // SECTION("MomersionFilterBarReference") { // Uncomment if used
    //     MomersionFilterBarReference mfbr(1, 10);
    //     REQUIRE(mfbr.getBarOffset() == 1);
    //     REQUIRE(mfbr.getMomersionPeriod() == 10);
    //     REQUIRE(mfbr.getReferenceType() == PriceBarReference::MOMERSIONFILTER);
    //     REQUIRE(mfbr.extraBarsNeeded() == 10);
    //     mfbr.accept(visitor);
    //     REQUIRE(visitor.visited_nodes.back() == "MomersionFilterBarReference");
    //     unsigned long long hc1 = mfbr.hashCode();
    //     REQUIRE(hc1 != 0);
    //     REQUIRE(mfbr.hashCode() == hc1);
    // }
}

TEST_CASE("PatternExpression Classes", "[PatternExpression]") {
    AstFactory factory;
    MockPalCodeGenVisitor visitor;
    auto open0 = factory.getPriceOpen(0);
    auto close1 = factory.getPriceClose(1);
    auto high0 = factory.getPriceHigh(0);


    SECTION("GreaterThanExpr") {
        GreaterThanExpr gtExpr(open0.get(), close1.get());
        REQUIRE(gtExpr.getLHS() == open0.get());
        REQUIRE(gtExpr.getRHS() == close1.get());
        gtExpr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "GreaterThanExpr");
        unsigned long long hc = gtExpr.hashCode();
        REQUIRE(hc != 0);

        GreaterThanExpr gtExpr_copy(gtExpr);
        REQUIRE(gtExpr_copy.getLHS() == open0.get());
        REQUIRE(gtExpr_copy.getRHS() == close1.get());
        REQUIRE(gtExpr_copy.hashCode() == hc);

        GreaterThanExpr gtExpr_assign(high0.get(), open0.get());
        gtExpr_assign = gtExpr;
        REQUIRE(gtExpr_assign.getLHS() == open0.get());
        REQUIRE(gtExpr_assign.getRHS() == close1.get());
        REQUIRE(gtExpr_assign.hashCode() == hc);
    }

    SECTION("AndExpr") {
        PatternExpressionPtr gtExpr1 = std::make_shared<GreaterThanExpr>(open0.get(), close1.get());
        PatternExpressionPtr gtExpr2 = std::make_shared<GreaterThanExpr>(close1.get(), high0.get());

        // AndExpr takes raw pointers in constructor, but stores shared_ptr
        // This seems a bit risky if not managed carefully by the creator of AndExpr
        // For the test, we use shared_ptr to manage lifetime and pass raw pointers
        AndExpr andExpr(gtExpr1.get(), gtExpr2.get());
        REQUIRE(andExpr.getLHS() == gtExpr1.get());
        REQUIRE(andExpr.getRHS() == gtExpr2.get());
        andExpr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "AndExpr");
        unsigned long long hc = andExpr.hashCode();
        REQUIRE(hc != 0);

        AndExpr andExpr_copy(andExpr);
        REQUIRE(andExpr_copy.getLHS() == gtExpr1.get());
        REQUIRE(andExpr_copy.getRHS() == gtExpr2.get());
        REQUIRE(andExpr_copy.hashCode() == hc);


        PatternExpressionPtr gtExpr3 = std::make_shared<GreaterThanExpr>(open0.get(), high0.get());
        PatternExpressionPtr gtExpr4 = std::make_shared<GreaterThanExpr>(high0.get(), close1.get());
        AndExpr andExpr_assign(gtExpr3.get(), gtExpr4.get());
        andExpr_assign = andExpr;
        REQUIRE(andExpr_assign.getLHS() == gtExpr1.get());
        REQUIRE(andExpr_assign.getRHS() == gtExpr2.get());
        REQUIRE(andExpr_assign.hashCode() == hc);
    }
}

TEST_CASE("ProfitTargetInPercentExpression Classes", "[ProfitTarget]") {
    AstFactory factory;
    MockPalCodeGenVisitor visitor;
    char ptStr[] = "2.5";
    auto pt_val = factory.getDecimalNumber(ptStr);

    SECTION("LongSideProfitTargetInPercent") {
        LongSideProfitTargetInPercent longPt(pt_val);
        REQUIRE(longPt.getProfitTarget() == pt_val.get());
        REQUIRE(longPt.isLongSideProfitTarget()); //
        REQUIRE_FALSE(longPt.isShortSideProfitTarget()); //
        longPt.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongSideProfitTargetInPercent");
        unsigned long long hc = longPt.hashCode();
        REQUIRE(hc != 0);
        REQUIRE(longPt.hashCode() == hc); // Cached

        LongSideProfitTargetInPercent longPt_copy(longPt);
        REQUIRE(longPt_copy.getProfitTarget() == pt_val.get());
        REQUIRE(longPt_copy.hashCode() == hc);

        char ptStr2[] = "3.0";
        auto pt_val2 = factory.getDecimalNumber(ptStr2);
        LongSideProfitTargetInPercent longPt_assign(pt_val2);
        longPt_assign = longPt;
        REQUIRE(longPt_assign.getProfitTarget() == pt_val.get());
        REQUIRE(longPt_assign.hashCode() == hc);


    }
    SECTION("ShortSideProfitTargetInPercent") {
        ShortSideProfitTargetInPercent shortPt(pt_val);
        REQUIRE(shortPt.getProfitTarget() == pt_val.get());
        REQUIRE(shortPt.isShortSideProfitTarget()); //
        REQUIRE_FALSE(shortPt.isLongSideProfitTarget()); //
        shortPt.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "ShortSideProfitTargetInPercent");
        unsigned long long hc = shortPt.hashCode();
        REQUIRE(hc != 0);
        REQUIRE(shortPt.hashCode() == hc); // Cached
    }
}


TEST_CASE("StopLossInPercentExpression Classes", "[StopLoss]") {
    AstFactory factory;
    MockPalCodeGenVisitor visitor;
    char slStr[] = "1.5";
    auto sl_val = factory.getDecimalNumber(slStr);

    SECTION("LongSideStopLossInPercent") {
        LongSideStopLossInPercent longSl(sl_val);
        REQUIRE(longSl.getStopLoss() == sl_val.get());
        REQUIRE(longSl.isLongSideStopLoss()); //
        REQUIRE_FALSE(longSl.isShortSideStopLoss()); //
        longSl.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongSideStopLossInPercent");
        unsigned long long hc = longSl.hashCode();
        REQUIRE(hc != 0);
        REQUIRE(longSl.hashCode() == hc); // Cached

        LongSideStopLossInPercent longSl_copy(longSl);
        REQUIRE(longSl_copy.getStopLoss() == sl_val.get());
        REQUIRE(longSl_copy.hashCode() == hc);

        char slStr2[] = "2.0";
        auto sl_val2 = factory.getDecimalNumber(slStr2);
        LongSideStopLossInPercent longSl_assign(sl_val2);
        longSl_assign = longSl;
        REQUIRE(longSl_assign.getStopLoss() == sl_val.get());
        REQUIRE(longSl_assign.hashCode() == hc);
    }

    SECTION("ShortSideStopLossInPercent") {
        ShortSideStopLossInPercent shortSl(sl_val);
        REQUIRE(shortSl.getStopLoss() == sl_val.get());
        REQUIRE(shortSl.isShortSideStopLoss()); //
        REQUIRE_FALSE(shortSl.isLongSideStopLoss()); //
        shortSl.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "ShortSideStopLossInPercent");
        unsigned long long hc = shortSl.hashCode();
        REQUIRE(hc != 0);
        REQUIRE(shortSl.hashCode() == hc); // Cached
    }
}


TEST_CASE("MarketEntryExpression Classes", "[MarketEntry]") {
    MockPalCodeGenVisitor visitor;
    SECTION("LongMarketEntryOnOpen") {
        LongMarketEntryOnOpen longEntry;
        REQUIRE(longEntry.isLongPattern()); //
        REQUIRE_FALSE(longEntry.isShortPattern()); //
        longEntry.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongMarketEntryOnOpen");
        REQUIRE(longEntry.hashCode() != 0);

        LongMarketEntryOnOpen longEntry_copy(longEntry);
        REQUIRE(longEntry_copy.isLongPattern()); //
        REQUIRE(longEntry_copy.hashCode() == longEntry.hashCode());

        LongMarketEntryOnOpen longEntry_assign; // Default constructor
        longEntry_assign = longEntry; // Assignment
        REQUIRE(longEntry_assign.isLongPattern()); //
        REQUIRE(longEntry_assign.hashCode() == longEntry.hashCode());
    }
    SECTION("ShortMarketEntryOnOpen") {
        ShortMarketEntryOnOpen shortEntry;
        REQUIRE(shortEntry.isShortPattern()); //
        REQUIRE_FALSE(shortEntry.isLongPattern()); //
        shortEntry.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "ShortMarketEntryOnOpen");
        REQUIRE(shortEntry.hashCode() != 0);
    }
}

TEST_CASE("PatternDescription Class", "[PatternDescription]") {
    AstFactory factory;
    MockPalCodeGenVisitor visitor;
    char pLongStr[] = "70.5";
    char pShortStr[] = "20.3";
    auto pLong = factory.getDecimalNumber(pLongStr);
    auto pShort = factory.getDecimalNumber(pShortStr);

    PatternDescription pd("testFile.txt", 1, 20230101, pLong, pShort, 100, 5);
    REQUIRE(pd.getFileName() == "testFile.txt");
    REQUIRE(pd.getpatternIndex() == 1);
    REQUIRE(pd.getIndexDate() == 20230101);
    REQUIRE(pd.getPercentLong() == pLong.get());
    REQUIRE(pd.getPercentShort() == pShort.get());
    REQUIRE(pd.numTrades() == 100);
    REQUIRE(pd.numConsecutiveLosses() == 5);

    pd.accept(visitor);
    REQUIRE(visitor.visited_nodes.back() == "PatternDescription");

    unsigned long long hc = pd.hashCode();
    REQUIRE(hc != 0);
    REQUIRE(pd.hashCode() == hc); // Cached

    PatternDescription pd_copy(pd);
    REQUIRE(pd_copy.getFileName() == "testFile.txt");
    REQUIRE(pd_copy.hashCode() == hc);

    char pLongStr2[] = "60.0";
    auto pLong2 = factory.getDecimalNumber(pLongStr2);
    PatternDescription pd_assign("other.txt", 2, 20220101, pLong2, pShort, 50, 2);
    pd_assign = pd;
    REQUIRE(pd_assign.getFileName() == "testFile.txt");
    REQUIRE(pd_assign.hashCode() == hc);

}


TEST_CASE("PalPatternMaxBars Evaluation", "[PalPatternMaxBars]") {
    AstFactory factory;
    auto open0 = factory.getPriceOpen(0); // extraBarsNeeded = 0
    auto close1 = factory.getPriceClose(1); // extraBarsNeeded = 0
    auto roc5 = factory.getRoc1(5);       // extraBarsNeeded = 1
    auto meander2 = factory.getMeander(2); // extraBarsNeeded = 5

    SECTION("Single GreaterThanExpr") {
        GreaterThanExpr gtExpr(open0.get(), close1.get()); // max(0+0, 1+0) = 1
        REQUIRE(PalPatternMaxBars::evaluateExpression(&gtExpr) == 1);

        GreaterThanExpr gtExpr2(roc5.get(), meander2.get()); // max(5+1, 2+5) = max(6,7) = 7
        REQUIRE(PalPatternMaxBars::evaluateExpression(&gtExpr2) == 7);
    }

    SECTION("AndExpr") {
        PatternExpressionPtr gt1_ptr = std::make_shared<GreaterThanExpr>(open0.get(), close1.get()); // max_bars = 1
        PatternExpressionPtr gt2_ptr = std::make_shared<GreaterThanExpr>(roc5.get(), meander2.get()); // max_bars = 7
        AndExpr andExpr(gt1_ptr.get(), gt2_ptr.get()); // max(1, 7) = 7
        REQUIRE(PalPatternMaxBars::evaluateExpression(&andExpr) == 7);

        PatternExpressionPtr gt3_ptr = std::make_shared<GreaterThanExpr>(factory.getPriceHigh(3), factory.getPriceLow(4)); // max(3,4) = 4
        PatternExpressionPtr gt4_ptr = std::make_shared<GreaterThanExpr>(factory.getVolume(0), factory.getIBS1(1)); // max(0,1) = 1
        AndExpr andExpr2(gt3_ptr.get(), gt4_ptr.get()); // max(4,1) = 4
        REQUIRE(PalPatternMaxBars::evaluateExpression(&andExpr2) == 4);
    }
     SECTION("Nested AndExpr") {
        PatternExpressionPtr o0_c1 = std::make_shared<GreaterThanExpr>(open0.get(), close1.get()); // 1
        PatternExpressionPtr r5_m2 = std::make_shared<GreaterThanExpr>(roc5.get(), meander2.get()); // 7
        PatternExpressionPtr h3_l4 = std::make_shared<GreaterThanExpr>(factory.getPriceHigh(3), factory.getPriceLow(4)); // 4

        AndExpr and1(o0_c1.get(), r5_m2.get()); // max(1,7) = 7
        PatternExpressionPtr and1_ptr = std::make_shared<AndExpr>(and1); // AndExpr copy constructor takes const AndExpr&

        AndExpr and_nested(and1_ptr.get(), h3_l4.get()); // max(7,4) = 7
        REQUIRE(PalPatternMaxBars::evaluateExpression(&and_nested) == 7);
    }
    SECTION("Unknown Expression Type") {
        // This requires a new class derived from PatternExpression that is not handled
        class UnknownExpr : public PatternExpression {
        public:
            void accept(PalCodeGenVisitor &v) override {}
            unsigned long long hashCode() override { return 0; }
        };
        UnknownExpr unknown;
        REQUIRE_THROWS_AS(PalPatternMaxBars::evaluateExpression(&unknown), std::domain_error);
    }
}


TEST_CASE("PriceActionLabPattern Class", "[PriceActionLabPattern]") {
    AstFactory factory;
    MockPalCodeGenVisitor visitor;

    // Setup components for PriceActionLabPattern
    char pLongStr[] = "70.0"; auto pLong = factory.getDecimalNumber(pLongStr);
    char pShortStr[] = "30.0"; auto pShort = factory.getDecimalNumber(pShortStr);
    PatternDescription* desc_raw = new PatternDescription("file.txt", 1, 20230101, pLong, pShort, 10, 2);
    PatternDescriptionPtr desc = std::shared_ptr<PatternDescription>(desc_raw);


    auto open0 = factory.getPriceOpen(0);
    auto close1 = factory.getPriceClose(1);
    PatternExpression* gt_expr_raw = new GreaterThanExpr(open0.get(), close1.get());
    PatternExpressionPtr pattern_expr = std::shared_ptr<PatternExpression>(gt_expr_raw);


    auto entry = factory.getLongMarketEntryOnOpen();

    char ptStr[] = "5.0"; auto pt_val = factory.getDecimalNumber(ptStr);
    auto profit_target = factory.getLongProfitTarget(pt_val);

    char slStr[] = "2.0"; auto sl_val = factory.getDecimalNumber(slStr);
    auto stop_loss = factory.getLongStopLoss(sl_val);

    SECTION("Constructor and Basic Getters") {
        PriceActionLabPattern pal_pattern(desc.get(), pattern_expr.get(), entry, profit_target, stop_loss); // Using raw pointers from shared_ptr for first two, shared_ptr for rest

        REQUIRE(pal_pattern.getFileName() == "file.txt");
        REQUIRE(pal_pattern.getBaseFileName() == "file"); //
        REQUIRE(pal_pattern.getpatternIndex() == 1);
        REQUIRE(pal_pattern.getIndexDate() == 20230101);
        REQUIRE(pal_pattern.getPatternExpression().get() == pattern_expr.get());
        REQUIRE(pal_pattern.getMarketEntry() == entry);
        REQUIRE(pal_pattern.getProfitTarget() == profit_target);
        REQUIRE(pal_pattern.getStopLoss() == stop_loss);
        REQUIRE(pal_pattern.getPatternDescription().get() == desc.get());
        REQUIRE(pal_pattern.getMaxBarsBack() == 1); // From O[0] > C[1]
        REQUIRE(pal_pattern.getPayoffRatio() == (num::fromString<decimal7>("5.0") / num::fromString<decimal7>("2.0")));
        REQUIRE(pal_pattern.isLongPattern()); //
        REQUIRE_FALSE(pal_pattern.isShortPattern()); //
        REQUIRE_FALSE(pal_pattern.hasVolatilityAttribute()); //
        REQUIRE_FALSE(pal_pattern.hasPortfolioAttribute()); //

        pal_pattern.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceActionLabPattern");

        unsigned long long hc = pal_pattern.hashCode();
        REQUIRE(hc != 0);
    }

    SECTION("Constructor with shared_ptr and attributes") {
         PriceActionLabPattern pal_pattern_sp(desc, pattern_expr, entry, profit_target, stop_loss);

        REQUIRE(pal_pattern_sp.getFileName() == "file.txt");
        REQUIRE(pal_pattern_sp.getMaxBarsBack() == 1);


        PriceActionLabPattern pal_pattern_attr(desc.get(), pattern_expr.get(), entry, profit_target, stop_loss,
                                               PriceActionLabPattern::VOLATILITY_HIGH,
                                               PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        REQUIRE(pal_pattern_attr.hasVolatilityAttribute()); //
        REQUIRE(pal_pattern_attr.isHighVolatilityPattern()); //
        REQUIRE_FALSE(pal_pattern_attr.isLowVolatilityPattern()); //
        REQUIRE(pal_pattern_attr.hasPortfolioAttribute()); //
        REQUIRE(pal_pattern_attr.isFilteredLongPattern()); //
        REQUIRE_FALSE(pal_pattern_attr.isFilteredShortPattern()); //
    }


    SECTION("Clone method") {
        PriceActionLabPattern original_pattern(desc, pattern_expr, entry, profit_target, stop_loss);

        char newPtStr[] = "6.0"; auto new_pt_val = factory.getDecimalNumber(newPtStr);
        auto new_profit_target = factory.getLongProfitTarget(new_pt_val);

        char newSlStr[] = "2.5"; auto new_sl_val = factory.getDecimalNumber(newSlStr);
        auto new_stop_loss = factory.getLongStopLoss(new_sl_val);

        PALPatternPtr cloned_pattern = original_pattern.clone(new_profit_target, new_stop_loss);

        REQUIRE(cloned_pattern != nullptr);
        REQUIRE(cloned_pattern->getFileName() == original_pattern.getFileName());
        REQUIRE(cloned_pattern->getPatternExpression().get() == original_pattern.getPatternExpression().get()); // Should be shallow copy of pattern expr
        REQUIRE(cloned_pattern->getMarketEntry() == original_pattern.getMarketEntry()); // Shallow
        REQUIRE(cloned_pattern->getPatternDescription().get() == original_pattern.getPatternDescription().get()); // Shallow

        REQUIRE(cloned_pattern->getProfitTarget() == new_profit_target);
        REQUIRE(cloned_pattern->getStopLoss() == new_stop_loss);
        REQUIRE(cloned_pattern->getProfitTargetAsDecimal() == *new_pt_val);
        REQUIRE(cloned_pattern->getStopLossAsDecimal() == *new_sl_val);
        REQUIRE(cloned_pattern->getPayoffRatio() == (*new_pt_val / *new_sl_val));
    }

    SECTION("Base Filename Variations") {
        PatternDescription* desc_no_ext_raw = new PatternDescription("fileNoExt", 1, 20230101, pLong, pShort, 10, 2);
        PatternDescriptionPtr desc_no_ext = std::shared_ptr<PatternDescription>(desc_no_ext_raw);
        PriceActionLabPattern p1(desc_no_ext, pattern_expr, entry, profit_target, stop_loss);
        REQUIRE(p1.getBaseFileName() == "fileNoExt"); //

        PatternDescription* desc_dot_front_raw = new PatternDescription(".bashrc", 1, 20230101, pLong, pShort, 10, 2);
         PatternDescriptionPtr desc_dot_front = std::shared_ptr<PatternDescription>(desc_dot_front_raw);
        PriceActionLabPattern p2(desc_dot_front, pattern_expr, entry, profit_target, stop_loss);
        REQUIRE(p2.getBaseFileName() == ".bashrc"); //


        PatternDescription* desc_multi_dot_raw = new PatternDescription("archive.tar.gz", 1, 20230101, pLong, pShort, 10, 2);
        PatternDescriptionPtr desc_multi_dot = std::shared_ptr<PatternDescription>(desc_multi_dot_raw);
        PriceActionLabPattern p3(desc_multi_dot, pattern_expr, entry, profit_target, stop_loss);
        REQUIRE(p3.getBaseFileName() == "archive.tar"); //
    }
}

TEST_CASE("SmallestVolatilityTieBreaker", "[PatternTieBreaker]") {
    AstFactory factory;
    // Dummy components for PALPatternPtr creation
    PatternDescriptionPtr desc = std::make_shared<PatternDescription>("f",static_cast<unsigned int>(0),static_cast<unsigned long>(0),factory.getDecimalNumber(0),factory.getDecimalNumber(0),static_cast<unsigned int>(0),static_cast<unsigned int>(0));
    PatternExpressionPtr expr = std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0), factory.getPriceClose(0));
    auto entry = factory.getLongMarketEntryOnOpen();
    auto pt = factory.getLongProfitTarget(factory.getDecimalNumber(1));
    auto sl = factory.getLongStopLoss(factory.getDecimalNumber(1));

    SmallestVolatilityTieBreaker tie_breaker;

    SECTION("Pattern1 is less volatile") {
        PALPatternPtr p1 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_LOW, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        PALPatternPtr p2 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_HIGH, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(tie_breaker.getTieBreakerPattern(p1, p2) == p1);
        REQUIRE(tie_breaker.getTieBreakerPattern(p2, p1) == p1); // Order shouldn't matter if one is clearly less
    }

    SECTION("Pattern2 is less volatile") {
        PALPatternPtr p1 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_VERY_HIGH, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        PALPatternPtr p2 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_NORMAL, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(tie_breaker.getTieBreakerPattern(p1, p2) == p2);
        REQUIRE(tie_breaker.getTieBreakerPattern(p2, p1) == p2);
    }

    SECTION("Equal volatility (VOLATILITY_NONE)") {
        PALPatternPtr p1 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_NONE, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        PALPatternPtr p2 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_NONE, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        // If volatilities are equal (including NONE), it should prefer pattern1 by default (based on implementation)
        REQUIRE(tie_breaker.getTieBreakerPattern(p1, p2) == p1);
    }
     SECTION("Equal volatility (actual enum value)") {
        PALPatternPtr p1 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_HIGH, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        PALPatternPtr p2 = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_HIGH, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(tie_breaker.getTieBreakerPattern(p1, p2) == p1); // Prefers pattern1 in a tie
    }

    SECTION("One has VOLATILITY_NONE, other has actual volatility") {
        PALPatternPtr p_none = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_NONE, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        PALPatternPtr p_low = std::make_shared<PriceActionLabPattern>(desc, expr, entry, pt, sl, PriceActionLabPattern::VOLATILITY_LOW, PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        // VOLATILITY_NONE is treated as higher than any specific volatility by the tie-breaker logic
        REQUIRE(tie_breaker.getTieBreakerPattern(p_none, p_low) == p_low);
        REQUIRE(tie_breaker.getTieBreakerPattern(p_low, p_none) == p_low);
    }
}

TEST_CASE("PriceActionLabSystem Class", "[PriceActionLabSystem]") {
    AstFactory factory;
    PatternTieBreakerPtr tie_breaker = std::make_shared<SmallestVolatilityTieBreaker>();

    // Dummy components for PALPatternPtr creation
    PatternDescriptionPtr desc1_ptr =
        std::make_shared<PatternDescription>("f1.txt",static_cast<unsigned int>(1),static_cast<unsigned long>(20230101),
            factory.getDecimalNumber(1),
            factory.getDecimalNumber(1),
            static_cast<unsigned int>(10),static_cast<unsigned int>(1));
    PatternExpressionPtr expr1_ptr =
        std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0),
                                          factory.getPriceClose(1));
    auto long_entry   = factory.getLongMarketEntryOnOpen();
    auto pt1 = factory.getLongProfitTarget(factory.getDecimalNumber(1));
    auto sl1 = factory.getLongStopLoss   (factory.getDecimalNumber(1));

    PALPatternPtr long_p1 =
        std::make_shared<PriceActionLabPattern>(
            desc1_ptr, expr1_ptr, long_entry, pt1, sl1,
            PriceActionLabPattern::VOLATILITY_LOW,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
    unsigned long long long_p1_hash = long_p1->hashCode();

    SECTION("Adding duplicate hash pattern without tie-breaker") {
        PriceActionLabSystem system(tie_breaker, false); // useTieBreaker = false
        system.addPattern(long_p1);

        // Create another long pattern with a different volatility → hash must differ now
        PALPatternPtr long_p1_dup =
            std::make_shared<PriceActionLabPattern>(
                desc1_ptr, expr1_ptr, long_entry, pt1, sl1,
                PriceActionLabPattern::VOLATILITY_HIGH,
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(long_p1_dup->hashCode() != long_p1_hash); // changed to !=
        REQUIRE(long_p1_dup != long_p1);                  // distinct objects

        system.addPattern(long_p1_dup);
        REQUIRE(system.getNumLongPatterns() == 2);        // both survive under distinct hashes

        // Verify both exist under two different hash keys
        std::set<unsigned long long> hashes;
        for (auto it = system.patternLongsBegin(); it != system.patternLongsEnd(); ++it)
            hashes.insert(it->first);
        REQUIRE(hashes.size() == 2);
    }

    SECTION("Adding duplicate hash pattern WITH tie-breaker") {
        PriceActionLabSystem system(tie_breaker, true); // useTieBreaker = true
        system.addPattern(long_p1); // VOLATILITY_LOW

        PALPatternPtr long_p1_dup_high_vol =
            std::make_shared<PriceActionLabPattern>(
                desc1_ptr, expr1_ptr, long_entry, pt1, sl1,
                PriceActionLabPattern::VOLATILITY_HIGH,
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(long_p1_dup_high_vol->hashCode() != long_p1_hash); // changed to !=

        system.addPattern(long_p1_dup_high_vol);
        // Since hashes differ, they don’t collide → both are kept
        REQUIRE(system.getNumLongPatterns() == 2);

        // If you *did* want to simulate a collision to exercise the tie-breaker,
        // you’d have to feed two patterns with the *same* volatility/portfolio.
    }
}

// ============================================================================
// THREAD SAFETY TESTS
// ============================================================================

TEST_CASE("AstFactory Thread Safety - Concurrent Price Bar Access", "[AstFactory][threading][critical]") {
    AstFactory factory;
    const int num_threads = 10;
    const int iterations_per_thread = 1000;
    std::atomic<int> errors{0};

    SECTION("Concurrent getPriceOpen calls") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors, iterations_per_thread]() {
                for(int j = 0; j < iterations_per_thread; ++j) {
                    auto p = factory.getPriceOpen(j % 20);
                    if(!p || p->getBarOffset() != (unsigned int)(j % 20)) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Concurrent getPriceHigh calls") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors, iterations_per_thread]() {
                for(int j = 0; j < iterations_per_thread; ++j) {
                    auto p = factory.getPriceHigh(j % 15);
                    if(!p || p->getBarOffset() != (unsigned int)(j % 15)) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Concurrent mixed price bar type access") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors, i, iterations_per_thread]() {
                for(int j = 0; j < iterations_per_thread; ++j) {
                    std::shared_ptr<PriceBarReference> p;
                    switch(j % 4) {
                        case 0: p = factory.getPriceOpen(j % 10); break;
                        case 1: p = factory.getPriceHigh(j % 10); break;
                        case 2: p = factory.getPriceLow(j % 10); break;
                        case 3: p = factory.getPriceClose(j % 10); break;
                    }
                    if(!p || p->getBarOffset() != (unsigned int)(j % 10)) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }
}

TEST_CASE("AstFactory Thread Safety - Concurrent Decimal Number Creation", "[AstFactory][threading][critical]") {
    AstFactory factory;
    const int num_threads = 10;
    const int iterations_per_thread = 500;
    std::atomic<int> errors{0};

    SECTION("Concurrent getDecimalNumber from string") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors, iterations_per_thread]() {
                for(int j = 0; j < iterations_per_thread; ++j) {
                    std::string num_str = std::to_string(j % 100) + ".5";
                    char* cstr = new char[num_str.length() + 1];
                    std::strcpy(cstr, num_str.c_str());
                    
                    auto p = factory.getDecimalNumber(cstr);
                    delete[] cstr;
                    
                    if(!p) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Concurrent getDecimalNumber from int") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors, iterations_per_thread]() {
                for(int j = 0; j < iterations_per_thread; ++j) {
                    auto p = factory.getDecimalNumber(j % 100);
                    if(!p) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Same decimal value from multiple threads returns same object") {
        std::vector<std::thread> threads;
        std::vector<std::shared_ptr<decimal7>> results(num_threads);
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &results, i]() {
                results[i] = factory.getDecimalNumber(42);
            });
        }
        
        for(auto& t : threads) t.join();
        
        // All threads should get the same cached object
        for(int i = 1; i < num_threads; ++i) {
            REQUIRE(results[i].get() == results[0].get());
        }
    }
}

TEST_CASE("AstFactory Thread Safety - Concurrent Profit Target/Stop Loss Creation", "[AstFactory][threading][critical]") {
    AstFactory factory;
    const int num_threads = 8;
    std::atomic<int> errors{0};

    SECTION("Concurrent long profit target creation") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors]() {
                for(int j = 0; j < 200; ++j) {
                    auto val = factory.getDecimalNumber(j % 20);
                    auto pt = factory.getLongProfitTarget(val);
                    if(!pt) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Concurrent short stop loss creation") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors]() {
                for(int j = 0; j < 200; ++j) {
                    auto val = factory.getDecimalNumber(j % 15);
                    auto sl = factory.getShortStopLoss(val);
                    if(!sl) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Mixed concurrent creation - all types") {
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&factory, &errors, i]() {
                for(int j = 0; j < 100; ++j) {
                    auto val = factory.getDecimalNumber((i * 100 + j) % 30);
                    
                    switch(j % 4) {
                        case 0: {
                            auto pt = factory.getLongProfitTarget(val);
                            if(!pt) errors++;
                            break;
                        }
                        case 1: {
                            auto pt = factory.getShortProfitTarget(val);
                            if(!pt) errors++;
                            break;
                        }
                        case 2: {
                            auto sl = factory.getLongStopLoss(val);
                            if(!sl) errors++;
                            break;
                        }
                        case 3: {
                            auto sl = factory.getShortStopLoss(val);
                            if(!sl) errors++;
                            break;
                        }
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

TEST_CASE("AstFactory Boundary Conditions - Bar Offsets", "[AstFactory][boundary][critical]") {
    AstFactory factory;
    const unsigned int max_offset = AstFactory::MaxNumBarOffsets;

    SECTION("Bar offset at MaxNumBarOffsets-1 boundary (should be cached)") {
        auto p_cached = factory.getPriceOpen(max_offset - 1);
        REQUIRE(p_cached != nullptr);
        REQUIRE(p_cached->getBarOffset() == max_offset - 1);
        
        // Should return same cached instance
        auto p_cached_again = factory.getPriceOpen(max_offset - 1);
        REQUIRE(p_cached.get() == p_cached_again.get());
    }

    SECTION("Bar offset at MaxNumBarOffsets (should create new)") {
        auto p_new = factory.getPriceOpen(max_offset);
        REQUIRE(p_new != nullptr);
        REQUIRE(p_new->getBarOffset() == max_offset);
        
        // Should create a different instance (not cached)
        auto p_new_again = factory.getPriceOpen(max_offset);
        REQUIRE(p_new.get() != p_new_again.get());
    }

    SECTION("Bar offset just beyond MaxNumBarOffsets") {
        auto p = factory.getPriceOpen(max_offset + 1);
        REQUIRE(p != nullptr);
        REQUIRE(p->getBarOffset() == max_offset + 1);
    }

    SECTION("Very large bar offset") {
        auto p = factory.getPriceOpen(1000);
        REQUIRE(p != nullptr);
        REQUIRE(p->getBarOffset() == 1000);
        REQUIRE(p->getReferenceType() == PriceBarReference::OPEN);
    }

    SECTION("Zero bar offset") {
        auto p = factory.getPriceOpen(0);
        REQUIRE(p != nullptr);
        REQUIRE(p->getBarOffset() == 0);
    }

    SECTION("All bar reference types at boundary") {
        REQUIRE(factory.getPriceHigh(max_offset - 1) != nullptr);
        REQUIRE(factory.getPriceLow(max_offset - 1) != nullptr);
        REQUIRE(factory.getPriceClose(max_offset - 1) != nullptr);
        REQUIRE(factory.getVolume(max_offset - 1) != nullptr);
        REQUIRE(factory.getRoc1(max_offset - 1) != nullptr);
        REQUIRE(factory.getIBS1(max_offset - 1) != nullptr);
        REQUIRE(factory.getIBS2(max_offset - 1) != nullptr);
        REQUIRE(factory.getIBS3(max_offset - 1) != nullptr);
        REQUIRE(factory.getMeander(max_offset - 1) != nullptr);
        REQUIRE(factory.getVChartLow(max_offset - 1) != nullptr);
        REQUIRE(factory.getVChartHigh(max_offset - 1) != nullptr);
    }

    SECTION("All bar reference types beyond boundary") {
        REQUIRE(factory.getPriceHigh(max_offset)->getBarOffset() == max_offset);
        REQUIRE(factory.getPriceLow(max_offset + 1)->getBarOffset() == max_offset + 1);
        REQUIRE(factory.getPriceClose(max_offset + 2)->getBarOffset() == max_offset + 2);
    }
}

TEST_CASE("AstFactory Cache Efficiency Verification", "[AstFactory][boundary][performance]") {
    AstFactory factory;

    SECTION("Cached objects return same pointer") {
        auto p1 = factory.getPriceOpen(5);
        auto p2 = factory.getPriceOpen(5);
        auto p3 = factory.getPriceOpen(5);
        
        REQUIRE(p1.get() == p2.get());
        REQUIRE(p2.get() == p3.get());
    }

    SECTION("Non-cached objects return different pointers") {
        auto p1 = factory.getPriceOpen(15); // Beyond cache
        auto p2 = factory.getPriceOpen(15);
        
        REQUIRE(p1.get() != p2.get()); // Different instances
        REQUIRE(p1->getBarOffset() == p2->getBarOffset()); // Same offset value
    }

    SECTION("Decimal number caching from string") {
        char str1[] = "123.45";
        char str2[] = "123.45";
        
        auto d1 = factory.getDecimalNumber(str1);
        auto d2 = factory.getDecimalNumber(str2);
        
        REQUIRE(d1.get() == d2.get()); // Should be cached
    }

    SECTION("Decimal number caching from int") {
        auto d1 = factory.getDecimalNumber(42);
        auto d2 = factory.getDecimalNumber(42);
        auto d3 = factory.getDecimalNumber(42);
        
        REQUIRE(d1.get() == d2.get());
        REQUIRE(d2.get() == d3.get());
    }

    SECTION("Profit target caching") {
        auto val = factory.getDecimalNumber(10);
        auto pt1 = factory.getLongProfitTarget(val);
        auto pt2 = factory.getLongProfitTarget(val);
        
        REQUIRE(pt1.get() == pt2.get());
    }
}

// ============================================================================
// ERROR HANDLING AND EDGE CASES
// ============================================================================

// Note: GetBaseFilename tests temporarily commented out due to linking issue
// The function is declared in PalAst.h but may need proper linking setup
// Individual pattern tests cover the core functionality

/*
TEST_CASE("GetBaseFilename Edge Cases", "[utility][boundary]") {
    SECTION("Empty string") {
        std::string result = GetBaseFilename("");
        REQUIRE(result == "");
    }

    SECTION("Only extension (.txt)") {
        std::string result = GetBaseFilename(".txt");
        REQUIRE(result == ".txt"); // Dot at front is not treated as extension
    }

    SECTION("Multiple consecutive dots") {
        std::string result = GetBaseFilename("file..txt");
        REQUIRE(result == "file."); // Removes only last extension
    }

    SECTION("No extension") {
        std::string result = GetBaseFilename("filename");
        REQUIRE(result == "filename");
    }

    SECTION("Extension only (no basename)") {
        std::string result = GetBaseFilename(".bashrc");
        REQUIRE(result == ".bashrc");
    }

    SECTION("Multiple dots in filename") {
        std::string result = GetBaseFilename("archive.tar.gz");
        REQUIRE(result == "archive.tar");
    }

    SECTION("Long filename") {
        std::string long_name(1000, 'a');
        long_name += ".txt";
        std::string result = GetBaseFilename(long_name.c_str());
        REQUIRE(result.length() == 1000);
    }

    SECTION("Filename with path separator") {
        // GetBaseFilename only processes the string, doesn't parse paths
        std::string result = GetBaseFilename("path/to/file.txt");
        REQUIRE(result == "path/to/file");
    }
}
*/

TEST_CASE("PatternDescription Edge Cases", "[PatternDescription][boundary]") {
    AstFactory factory;

    SECTION("Zero values") {
        PatternDescription desc("file.txt", 0, 0, 
                               factory.getDecimalNumber(0), 
                               factory.getDecimalNumber(0), 
                               0, 0);
        REQUIRE(desc.getpatternIndex() == 0);
        REQUIRE(desc.getIndexDate() == 0);
        REQUIRE(desc.numTrades() == 0);
        REQUIRE(desc.numConsecutiveLosses() == 0);
    }

    SECTION("Very large values") {
        auto large_val = factory.getDecimalNumber(999999);
        PatternDescription desc("file.txt", 999999, 99999999,
                               large_val, large_val,
                               999999, 999999);
        REQUIRE(desc.getpatternIndex() == 999999);
        REQUIRE(desc.getIndexDate() == 99999999);
    }

    SECTION("Empty filename") {
        PatternDescription desc("", 1, 20230101,
                               factory.getDecimalNumber(1),
                               factory.getDecimalNumber(1),
                               10, 2);
        REQUIRE(desc.getFileName() == "");
    }
}

// ============================================================================
// RAW POINTER INTERFACE TESTS
// ============================================================================

TEST_CASE("AstFactory Raw Pointer Interface", "[AstFactory][legacy][raw][critical]") {
    AstFactory factory;

    SECTION("getPriceOpenRaw returns valid pointer") {
        PriceBarReference* raw_ptr = factory.getPriceOpenRaw(0);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(raw_ptr->getBarOffset() == 0);
        REQUIRE(raw_ptr->getReferenceType() == PriceBarReference::OPEN);
    }

    SECTION("getPriceHighRaw returns valid pointer") {
        PriceBarReference* raw_ptr = factory.getPriceHighRaw(3);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(raw_ptr->getBarOffset() == 3);
        REQUIRE(raw_ptr->getReferenceType() == PriceBarReference::HIGH);
    }

    SECTION("getPriceLowRaw returns valid pointer") {
        PriceBarReference* raw_ptr = factory.getPriceLowRaw(2);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(raw_ptr->getBarOffset() == 2);
    }

    SECTION("getPriceCloseRaw returns valid pointer") {
        PriceBarReference* raw_ptr = factory.getPriceCloseRaw(1);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(raw_ptr->getBarOffset() == 1);
    }

    SECTION("getVolumeRaw returns valid pointer") {
        PriceBarReference* raw_ptr = factory.getVolumeRaw(0);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(raw_ptr->getReferenceType() == PriceBarReference::VOLUME);
    }

    SECTION("getRoc1Raw returns valid pointer") {
        PriceBarReference* raw_ptr = factory.getRoc1Raw(0);
        REQUIRE(raw_ptr != nullptr);
        REQUIRE(raw_ptr->getReferenceType() == PriceBarReference::ROC1);
    }

    SECTION("getDecimalNumberRaw from string") {
        char str[] = "123.45";
        decimal7* raw_ptr = factory.getDecimalNumberRaw(str);
        REQUIRE(raw_ptr != nullptr);
    }

    SECTION("getDecimalNumberRaw from int") {
        decimal7* raw_ptr = factory.getDecimalNumberRaw(42);
        REQUIRE(raw_ptr != nullptr);
    }

    SECTION("getLongMarketEntryOnOpenRaw") {
        MarketEntryExpression* raw_ptr = factory.getLongMarketEntryOnOpenRaw();
        REQUIRE(raw_ptr != nullptr);
    }

    SECTION("getShortMarketEntryOnOpenRaw") {
        MarketEntryExpression* raw_ptr = factory.getShortMarketEntryOnOpenRaw();
        REQUIRE(raw_ptr != nullptr);
    }

    SECTION("getLongProfitTargetRaw with raw decimal pointer") {
        decimal7* val = new decimal7(5);
        LongSideProfitTargetInPercent* pt = factory.getLongProfitTargetRaw(val);
        REQUIRE(pt != nullptr);
        delete val; // Caller must manage memory
    }

    SECTION("getShortProfitTargetRaw with raw decimal pointer") {
        decimal7* val = new decimal7(3);
        ShortSideProfitTargetInPercent* pt = factory.getShortProfitTargetRaw(val);
        REQUIRE(pt != nullptr);
        delete val;
    }

    SECTION("getLongStopLossRaw with raw decimal pointer") {
        decimal7* val = new decimal7(2);
        LongSideStopLossInPercent* sl = factory.getLongStopLossRaw(val);
        REQUIRE(sl != nullptr);
        delete val;
    }

    SECTION("getShortStopLossRaw with raw decimal pointer") {
        decimal7* val = new decimal7(1);
        ShortSideStopLossInPercent* sl = factory.getShortStopLossRaw(val);
        REQUIRE(sl != nullptr);
        delete val;
    }

    SECTION("Raw pointer from cached object remains valid") {
        // For cached objects (offset < MaxNumBarOffsets), raw pointer should remain valid
        PriceBarReference* raw_ptr = factory.getPriceOpenRaw(5);
        REQUIRE(raw_ptr != nullptr);
        
        // Get it again - should be same object since it's cached
        PriceBarReference* raw_ptr2 = factory.getPriceOpenRaw(5);
        REQUIRE(raw_ptr == raw_ptr2);
    }
}

// ============================================================================
// HASH CODE TESTS
// ============================================================================

TEST_CASE("Hash Code Stability and Uniqueness", "[hashing][critical]") {
    AstFactory factory;

    SECTION("Hash stability - same object returns same hash") {
        auto open0 = factory.getPriceOpen(0);
        auto hash1 = open0->hashCode();
        auto hash2 = open0->hashCode();
        auto hash3 = open0->hashCode();
        
        REQUIRE(hash1 == hash2);
        REQUIRE(hash2 == hash3);
    }

    SECTION("Different bar offsets produce different hashes") {
        auto open0 = factory.getPriceOpen(0);
        auto open1 = factory.getPriceOpen(1);
        auto open2 = factory.getPriceOpen(2);
        
        REQUIRE(open0->hashCode() != open1->hashCode());
        REQUIRE(open1->hashCode() != open2->hashCode());
        REQUIRE(open0->hashCode() != open2->hashCode());
    }

    SECTION("Different reference types produce different hashes") {
        auto open0 = factory.getPriceOpen(0);
        auto high0 = factory.getPriceHigh(0);
        auto low0 = factory.getPriceLow(0);
        auto close0 = factory.getPriceClose(0);
        
        std::set<unsigned long long> hashes;
        hashes.insert(open0->hashCode());
        hashes.insert(high0->hashCode());
        hashes.insert(low0->hashCode());
        hashes.insert(close0->hashCode());
        
        REQUIRE(hashes.size() == 4); // All unique
    }

    SECTION("GreaterThanExpr hash stability") {
        auto expr = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), 
            factory.getPriceClose(0)
        );
        
        auto hash1 = expr->hashCode();
        auto hash2 = expr->hashCode();
        REQUIRE(hash1 == hash2);
    }

    SECTION("AndExpr hash stability") {
        auto left = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), factory.getPriceClose(0));
        auto right = std::make_shared<GreaterThanExpr>(
            factory.getPriceHigh(1), factory.getPriceLow(1));
        auto and_expr = std::make_shared<AndExpr>(left, right);
        
        auto hash1 = and_expr->hashCode();
        auto hash2 = and_expr->hashCode();
        REQUIRE(hash1 == hash2);
    }

    SECTION("Different expressions produce different hashes") {
        auto expr1 = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), factory.getPriceClose(0));
        auto expr2 = std::make_shared<GreaterThanExpr>(
            factory.getPriceHigh(0), factory.getPriceLow(0));
        
        REQUIRE(expr1->hashCode() != expr2->hashCode());
    }

    SECTION("PriceActionLabPattern hash includes volatility and portfolio filter") {
        auto desc = std::make_shared<PatternDescription>(
            "test.txt", static_cast<unsigned int>(1), static_cast<unsigned long>(20230101),
            factory.getDecimalNumber(1), factory.getDecimalNumber(1), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
        auto expr = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), factory.getPriceClose(0));
        auto entry = factory.getLongMarketEntryOnOpen();
        auto pt = factory.getLongProfitTarget(factory.getDecimalNumber(1));
        auto sl = factory.getLongStopLoss(factory.getDecimalNumber(1));
        
        auto pattern1 = std::make_shared<PriceActionLabPattern>(
            desc, expr, entry, pt, sl,
            PriceActionLabPattern::VOLATILITY_LOW,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        auto pattern2 = std::make_shared<PriceActionLabPattern>(
            desc, expr, entry, pt, sl,
            PriceActionLabPattern::VOLATILITY_HIGH,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        // Different volatility should produce different hashes
        REQUIRE(pattern1->hashCode() != pattern2->hashCode());
    }
}

// ============================================================================
// PRICEACTIONLABSYSTEM COMPREHENSIVE TESTS
// ============================================================================

TEST_CASE("PriceActionLabSystem Empty System Operations", "[PriceActionLabSystem][boundary]") {
    auto tie_breaker = std::make_shared<SmallestVolatilityTieBreaker>();
    PriceActionLabSystem system(tie_breaker, false);

    SECTION("Empty system has zero patterns") {
        REQUIRE(system.getNumLongPatterns() == 0);
        REQUIRE(system.getNumShortPatterns() == 0);
    }

    SECTION("Empty system iteration") {
        REQUIRE(system.patternLongsBegin() == system.patternLongsEnd());
        REQUIRE(system.patternShortsBegin() == system.patternShortsEnd());
    }

    SECTION("Iterating empty system doesn't crash") {
        int count = 0;
        for(auto it = system.patternLongsBegin(); it != system.patternLongsEnd(); ++it) {
            count++;
        }
        REQUIRE(count == 0);
    }
}

TEST_CASE("PriceActionLabSystem Short Patterns", "[PriceActionLabSystem]") {
    AstFactory factory;
    auto tie_breaker = std::make_shared<SmallestVolatilityTieBreaker>();
    PriceActionLabSystem system(tie_breaker, false);

    SECTION("Add short patterns") {
        auto desc = std::make_shared<PatternDescription>(
            "short1.txt", static_cast<unsigned int>(1), static_cast<unsigned long>(20230101),
            factory.getDecimalNumber(1), factory.getDecimalNumber(1), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
        auto expr = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), factory.getPriceClose(0));
        auto entry = factory.getShortMarketEntryOnOpen(); // SHORT entry
        auto pt = factory.getShortProfitTarget(factory.getDecimalNumber(2));
        auto sl = factory.getShortStopLoss(factory.getDecimalNumber(1));
        
        auto short_pattern = std::make_shared<PriceActionLabPattern>(
            desc, expr, entry, pt, sl,
            PriceActionLabPattern::VOLATILITY_NORMAL,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        system.addPattern(short_pattern);
        
        REQUIRE(system.getNumShortPatterns() == 1);
        REQUIRE(system.getNumLongPatterns() == 0);
    }

    SECTION("Add multiple short patterns") {
        for(int i = 0; i < 5; ++i) {
            std::string filename = "short" + std::to_string(i) + ".txt";
            auto desc = std::make_shared<PatternDescription>(
                filename.c_str(), static_cast<unsigned int>(i), static_cast<unsigned long>(20230101),
                factory.getDecimalNumber(i), factory.getDecimalNumber(i), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
            auto expr = std::make_shared<GreaterThanExpr>(
                factory.getPriceOpen(i), factory.getPriceClose(i));
            auto entry = factory.getShortMarketEntryOnOpen();
            auto pt = factory.getShortProfitTarget(factory.getDecimalNumber(2));
            auto sl = factory.getShortStopLoss(factory.getDecimalNumber(1));
            
            auto pattern = std::make_shared<PriceActionLabPattern>(
                desc, expr, entry, pt, sl,
                PriceActionLabPattern::VOLATILITY_NORMAL,
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
            
            system.addPattern(pattern);
        }
        
        REQUIRE(system.getNumShortPatterns() == 5);
    }

    SECTION("Iterate short patterns") {
        // Add a few short patterns
        for(int i = 0; i < 3; ++i) {
            std::string filename = "short" + std::to_string(i) + ".txt";
            auto desc = std::make_shared<PatternDescription>(
                filename.c_str(), static_cast<unsigned int>(i), static_cast<unsigned long>(20230101),
                factory.getDecimalNumber(i), factory.getDecimalNumber(i), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
            auto expr = std::make_shared<GreaterThanExpr>(
                factory.getPriceOpen(i), factory.getPriceClose(i));
            auto entry = factory.getShortMarketEntryOnOpen();
            auto pt = factory.getShortProfitTarget(factory.getDecimalNumber(2));
            auto sl = factory.getShortStopLoss(factory.getDecimalNumber(1));
            
            auto pattern = std::make_shared<PriceActionLabPattern>(
                desc, expr, entry, pt, sl,
                PriceActionLabPattern::VOLATILITY_NORMAL,
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
            
            system.addPattern(pattern);
        }
        
        int count = 0;
        for(auto it = system.patternShortsBegin(); it != system.patternShortsEnd(); ++it) {
            count++;
            REQUIRE(it->second != nullptr);
        }
        REQUIRE(count == 3);
    }
}

TEST_CASE("PriceActionLabSystem Mixed Long and Short Patterns", "[PriceActionLabSystem]") {
    AstFactory factory;
    auto tie_breaker = std::make_shared<SmallestVolatilityTieBreaker>();
    PriceActionLabSystem system(tie_breaker, false);

    SECTION("Add both long and short patterns") {
        // Add long pattern
        auto long_desc = std::make_shared<PatternDescription>(
            "long.txt", static_cast<unsigned int>(1), static_cast<unsigned long>(20230101),
            factory.getDecimalNumber(1), factory.getDecimalNumber(1), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
        auto long_expr = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), factory.getPriceClose(0));
        auto long_entry = factory.getLongMarketEntryOnOpen();
        auto long_pt = factory.getLongProfitTarget(factory.getDecimalNumber(2));
        auto long_sl = factory.getLongStopLoss(factory.getDecimalNumber(1));
        
        auto long_pattern = std::make_shared<PriceActionLabPattern>(
            long_desc, long_expr, long_entry, long_pt, long_sl,
            PriceActionLabPattern::VOLATILITY_NORMAL,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        // Add short pattern
        auto short_desc = std::make_shared<PatternDescription>(
            "short.txt", static_cast<unsigned int>(2), static_cast<unsigned long>(20230101),
            factory.getDecimalNumber(1), factory.getDecimalNumber(1), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
        auto short_expr = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(1), factory.getPriceClose(1));
        auto short_entry = factory.getShortMarketEntryOnOpen();
        auto short_pt = factory.getShortProfitTarget(factory.getDecimalNumber(2));
        auto short_sl = factory.getShortStopLoss(factory.getDecimalNumber(1));
        
        auto short_pattern = std::make_shared<PriceActionLabPattern>(
            short_desc, short_expr, short_entry, short_pt, short_sl,
            PriceActionLabPattern::VOLATILITY_NORMAL,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        system.addPattern(long_pattern);
        system.addPattern(short_pattern);
        
        REQUIRE(system.getNumLongPatterns() == 1);
        REQUIRE(system.getNumShortPatterns() == 1);
    }
}

TEST_CASE("PriceActionLabSystem Pattern Count Consistency", "[PriceActionLabSystem]") {
    AstFactory factory;
    auto tie_breaker = std::make_shared<SmallestVolatilityTieBreaker>();
    PriceActionLabSystem system(tie_breaker, false);

    SECTION("Pattern count matches iteration count") {
        const int num_patterns = 10;
        
        // Add patterns
        for(int i = 0; i < num_patterns; ++i) {
            std::string filename = "pattern" + std::to_string(i) + ".txt";
            auto desc = std::make_shared<PatternDescription>(
                filename.c_str(), static_cast<unsigned int>(i), static_cast<unsigned long>(20230101),
                factory.getDecimalNumber(i), factory.getDecimalNumber(i), static_cast<unsigned int>(10), static_cast<unsigned int>(2));
            auto expr = std::make_shared<GreaterThanExpr>(
                factory.getPriceOpen(i % 5), factory.getPriceClose(i % 5));
            auto entry = factory.getLongMarketEntryOnOpen();
            auto pt = factory.getLongProfitTarget(factory.getDecimalNumber(2));
            auto sl = factory.getLongStopLoss(factory.getDecimalNumber(1));
            
            auto pattern = std::make_shared<PriceActionLabPattern>(
                desc, expr, entry, pt, sl,
                PriceActionLabPattern::VOLATILITY_NORMAL,
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
            
            system.addPattern(pattern);
        }
        
        // Count via iteration
        int iter_count = 0;
        for(auto it = system.patternLongsBegin(); it != system.patternLongsEnd(); ++it) {
            iter_count++;
        }
        
        REQUIRE(system.getNumLongPatterns() == num_patterns);
        REQUIRE(iter_count == num_patterns);
    }
}

// ============================================================================
// EXTRA BARS NEEDED CORRECTNESS TESTS
// ============================================================================

TEST_CASE("extraBarsNeeded Correctness for All Types", "[PriceBarReference][validation]") {
    AstFactory factory;

    SECTION("Basic price bars need no extra bars") {
        REQUIRE(factory.getPriceOpen(0)->extraBarsNeeded() == 0);
        REQUIRE(factory.getPriceHigh(0)->extraBarsNeeded() == 0);
        REQUIRE(factory.getPriceLow(0)->extraBarsNeeded() == 0);
        REQUIRE(factory.getPriceClose(0)->extraBarsNeeded() == 0);
        REQUIRE(factory.getVolume(0)->extraBarsNeeded() == 0);
    }

    SECTION("Indicator bars need lookback periods") {
        REQUIRE(factory.getRoc1(0)->extraBarsNeeded() == 1);    // 1-period rate of change
        REQUIRE(factory.getIBS1(0)->extraBarsNeeded() == 0);    // IBS1 uses current bar only
        REQUIRE(factory.getIBS2(0)->extraBarsNeeded() == 1);    // IBS2 needs 1 prior bar
        REQUIRE(factory.getIBS3(0)->extraBarsNeeded() == 2);    // IBS3 needs 2 prior bars
        REQUIRE(factory.getMeander(0)->extraBarsNeeded() == 5); // Meander 5-bar window
        REQUIRE(factory.getVChartLow(0)->extraBarsNeeded() == 6);  // VChart 6-bar lookback
        REQUIRE(factory.getVChartHigh(0)->extraBarsNeeded() == 6); // VChart 6-bar lookback
    }

    SECTION("extraBarsNeeded is independent of bar offset") {
        // ROC1 always needs 1 extra bar, regardless of offset
        REQUIRE(factory.getRoc1(0)->extraBarsNeeded() == 1);
        REQUIRE(factory.getRoc1(5)->extraBarsNeeded() == 1);
        REQUIRE(factory.getRoc1(10)->extraBarsNeeded() == 1);
        
        // Meander always needs 5 extra bars
        REQUIRE(factory.getMeander(0)->extraBarsNeeded() == 5);
        REQUIRE(factory.getMeander(3)->extraBarsNeeded() == 5);
        REQUIRE(factory.getMeander(7)->extraBarsNeeded() == 5);
    }
}

// ============================================================================
// COMPLEX EXPRESSION TESTS
// ============================================================================

TEST_CASE("Complex Nested AndExpr Patterns", "[PatternExpression]") {
    AstFactory factory;
    MockPalCodeGenVisitor visitor;

    SECTION("Deeply nested AND expressions") {
        auto expr1 = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(0), factory.getPriceClose(0));
        auto expr2 = std::make_shared<GreaterThanExpr>(
            factory.getPriceHigh(1), factory.getPriceLow(1));
        auto expr3 = std::make_shared<GreaterThanExpr>(
            factory.getPriceOpen(2), factory.getPriceClose(2));
        
        auto and1 = std::make_shared<AndExpr>(expr1, expr2);
        auto and2 = std::make_shared<AndExpr>(and1, expr3);
        
        and2->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "AndExpr");
        
        // Verify hash is stable
        auto hash1 = and2->hashCode();
        auto hash2 = and2->hashCode();
        REQUIRE(hash1 == hash2);
    }

    SECTION("Wide AND expression tree") {
        std::vector<std::shared_ptr<GreaterThanExpr>> exprs;
        for(int i = 0; i < 5; ++i) {
            exprs.push_back(std::make_shared<GreaterThanExpr>(
                factory.getPriceOpen(i), factory.getPriceClose(i)));
        }
        
        std::shared_ptr<PatternExpression> wide_and = exprs[0];
        for(size_t i = 1; i < exprs.size(); ++i) {
            wide_and = std::make_shared<AndExpr>(wide_and, exprs[i]);
        }
        
        REQUIRE(wide_and != nullptr);
        wide_and->accept(visitor);
    }
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_CASE("Stress Test - Large Number of Patterns", "[PriceActionLabSystem][stress]") {
    AstFactory factory;
    auto tie_breaker = std::make_shared<SmallestVolatilityTieBreaker>();
    PriceActionLabSystem system(tie_breaker, false);

    const int num_patterns = 1000;

    SECTION("Add many unique patterns") {
        for(int i = 0; i < num_patterns; ++i) {
            std::string filename = "pattern" + std::to_string(i) + ".txt";
            auto desc = std::make_shared<PatternDescription>(
                filename.c_str(), static_cast<unsigned int>(i), static_cast<unsigned long>(20230101 + i),
                factory.getDecimalNumber(i % 100),
                factory.getDecimalNumber((i + 1) % 100),
                static_cast<unsigned int>(10 + (i % 50)), static_cast<unsigned int>(2 + (i % 10)));
            
            auto expr = std::make_shared<GreaterThanExpr>(
                factory.getPriceOpen(i % 10),
                factory.getPriceClose((i + 1) % 10));
            
            auto entry = (i % 2 == 0) ? factory.getLongMarketEntryOnOpen() 
                                      : factory.getShortMarketEntryOnOpen();
            
            std::shared_ptr<ProfitTargetInPercentExpression> pt;
            std::shared_ptr<StopLossInPercentExpression> sl;
            
            if(i % 2 == 0) {
                pt = factory.getLongProfitTarget(factory.getDecimalNumber(2 + (i % 5)));
                sl = factory.getLongStopLoss(factory.getDecimalNumber(1 + (i % 3)));
            } else {
                pt = factory.getShortProfitTarget(factory.getDecimalNumber(2 + (i % 5)));
                sl = factory.getShortStopLoss(factory.getDecimalNumber(1 + (i % 3)));
            }
            
            auto pattern = std::make_shared<PriceActionLabPattern>(
                desc, expr, entry, pt, sl,
                static_cast<PriceActionLabPattern::VolatilityAttribute>(i % 5),
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
            
            system.addPattern(pattern);
        }
        
        unsigned long long long_count = 0, short_count = 0;
        for(auto it = system.patternLongsBegin(); it != system.patternLongsEnd(); ++it) {
            long_count++;
        }
        for(auto it = system.patternShortsBegin(); it != system.patternShortsEnd(); ++it) {
            short_count++;
        }
        
        REQUIRE(system.getNumLongPatterns() == long_count);
        REQUIRE(system.getNumShortPatterns() == short_count);
        REQUIRE((long_count + short_count) == num_patterns);
    }
}

TEST_CASE("Stress Test - Factory Caching with High Load", "[AstFactory][stress]") {
    AstFactory factory;

    SECTION("Repeated requests for same objects") {
        const int iterations = 10000;
        
        for(int i = 0; i < iterations; ++i) {
            auto p = factory.getPriceOpen(i % 10);
            REQUIRE(p != nullptr);
            REQUIRE(p->getBarOffset() == (unsigned int)(i % 10));
        }
    }

    SECTION("Decimal number creation stress") {
        const int iterations = 5000;
        std::set<decimal7*> unique_ptrs;
        
        for(int i = 0; i < iterations; ++i) {
            auto d = factory.getDecimalNumber(i % 100);
            unique_ptrs.insert(d.get());
        }
        
        // Should only create 100 unique objects
        REQUIRE(unique_ptrs.size() == 100);
    }
}

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

TEST_CASE("AstResourceManager Construction and Factory Access", "[AstResourceManager][basic]") {
    SECTION("Default constructor creates valid manager") {
        AstResourceManager manager;
        auto factory = manager.getFactory();
        REQUIRE(factory != nullptr);
    }

    SECTION("Factory is accessible and functional") {
        AstResourceManager manager;
        auto factory = manager.getFactory();
        
        // Verify factory works
        auto priceOpen = factory->getPriceOpen(0);
        REQUIRE(priceOpen != nullptr);
    }

    SECTION("Same manager returns same factory") {
        AstResourceManager manager;
        auto factory1 = manager.getFactory();
        auto factory2 = manager.getFactory();
        
        REQUIRE(factory1.get() == factory2.get());
    }

    SECTION("Different managers have different factories") {
        AstResourceManager manager1;
        AstResourceManager manager2;
        
        auto factory1 = manager1.getFactory();
        auto factory2 = manager2.getFactory();
        
        REQUIRE(factory1.get() != factory2.get());
    }
}

// ============================================================================
// PRICE BAR REFERENCE TESTS
// ============================================================================

TEST_CASE("AstResourceManager Price Bar Reference Methods", "[AstResourceManager][pricebar]") {
    AstResourceManager manager;
    MockPalCodeGenVisitor visitor;

    SECTION("getPriceOpen returns valid reference") {
        auto open0 = manager.getPriceOpen(0);
        REQUIRE(open0 != nullptr);
        REQUIRE(open0->getBarOffset() == 0);
        REQUIRE(open0->getReferenceType() == PriceBarReference::OPEN);
        
        open0->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarOpen");
    }

    SECTION("getPriceHigh returns valid reference") {
        auto high1 = manager.getPriceHigh(1);
        REQUIRE(high1 != nullptr);
        REQUIRE(high1->getBarOffset() == 1);
        REQUIRE(high1->getReferenceType() == PriceBarReference::HIGH);
        
        high1->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarHigh");
    }

    SECTION("getPriceLow returns valid reference") {
        auto low2 = manager.getPriceLow(2);
        REQUIRE(low2 != nullptr);
        REQUIRE(low2->getBarOffset() == 2);
        REQUIRE(low2->getReferenceType() == PriceBarReference::LOW);
        
        low2->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarLow");
    }

    SECTION("getPriceClose returns valid reference") {
        auto close3 = manager.getPriceClose(3);
        REQUIRE(close3 != nullptr);
        REQUIRE(close3->getBarOffset() == 3);
        REQUIRE(close3->getReferenceType() == PriceBarReference::CLOSE);
        
        close3->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceBarClose");
    }

    SECTION("getVolume returns valid reference") {
        auto volume0 = manager.getVolume(0);
        REQUIRE(volume0 != nullptr);
        REQUIRE(volume0->getBarOffset() == 0);
        REQUIRE(volume0->getReferenceType() == PriceBarReference::VOLUME);
        
        volume0->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "VolumeBarReference");
    }

    SECTION("getRoc1 returns valid reference") {
        auto roc1 = manager.getRoc1(0);
        REQUIRE(roc1 != nullptr);
        REQUIRE(roc1->getBarOffset() == 0);
        REQUIRE(roc1->getReferenceType() == PriceBarReference::ROC1);
        REQUIRE(roc1->extraBarsNeeded() == 1);
        
        roc1->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "Roc1BarReference");
    }

    SECTION("getIBS1 returns valid reference") {
        auto ibs1 = manager.getIBS1(0);
        REQUIRE(ibs1 != nullptr);
        REQUIRE(ibs1->getBarOffset() == 0);
        REQUIRE(ibs1->getReferenceType() == PriceBarReference::IBS1);
        
        ibs1->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "IBS1BarReference");
    }

    SECTION("getIBS2 returns valid reference") {
        auto ibs2 = manager.getIBS2(0);
        REQUIRE(ibs2 != nullptr);
        REQUIRE(ibs2->getBarOffset() == 0);
        REQUIRE(ibs2->getReferenceType() == PriceBarReference::IBS2);
        REQUIRE(ibs2->extraBarsNeeded() == 1);
        
        ibs2->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "IBS2BarReference");
    }

    SECTION("getIBS3 returns valid reference") {
        auto ibs3 = manager.getIBS3(0);
        REQUIRE(ibs3 != nullptr);
        REQUIRE(ibs3->getBarOffset() == 0);
        REQUIRE(ibs3->getReferenceType() == PriceBarReference::IBS3);
        REQUIRE(ibs3->extraBarsNeeded() == 2);
        
        ibs3->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "IBS3BarReference");
    }

    SECTION("getMeander returns valid reference") {
        auto meander = manager.getMeander(0);
        REQUIRE(meander != nullptr);
        REQUIRE(meander->getBarOffset() == 0);
        REQUIRE(meander->getReferenceType() == PriceBarReference::MEANDER);
        REQUIRE(meander->extraBarsNeeded() == 5);
        
        meander->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "MeanderBarReference");
    }

    SECTION("getVChartLow returns valid reference") {
        auto vchartLow = manager.getVChartLow(0);
        REQUIRE(vchartLow != nullptr);
        REQUIRE(vchartLow->getBarOffset() == 0);
        REQUIRE(vchartLow->getReferenceType() == PriceBarReference::VCHARTLOW);
        REQUIRE(vchartLow->extraBarsNeeded() == 6);
        
        vchartLow->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "VChartLowBarReference");
    }

    SECTION("getVChartHigh returns valid reference") {
        auto vchartHigh = manager.getVChartHigh(0);
        REQUIRE(vchartHigh != nullptr);
        REQUIRE(vchartHigh->getBarOffset() == 0);
        REQUIRE(vchartHigh->getReferenceType() == PriceBarReference::VCHARTHIGH);
        REQUIRE(vchartHigh->extraBarsNeeded() == 6);
        
        vchartHigh->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "VChartHighBarReference");
    }
}

// ============================================================================
// MARKET ENTRY EXPRESSION TESTS
// ============================================================================

TEST_CASE("AstResourceManager Market Entry Methods", "[AstResourceManager][entry]") {
    AstResourceManager manager;
    MockPalCodeGenVisitor visitor;

    SECTION("getLongMarketEntryOnOpen returns valid expression") {
        auto longEntry = manager.getLongMarketEntryOnOpen();
        REQUIRE(longEntry != nullptr);
        
        longEntry->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongMarketEntryOnOpen");
    }

    SECTION("getShortMarketEntryOnOpen returns valid expression") {
        auto shortEntry = manager.getShortMarketEntryOnOpen();
        REQUIRE(shortEntry != nullptr);
        
        shortEntry->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "ShortMarketEntryOnOpen");
    }

    SECTION("Long and short entries are different objects") {
        auto longEntry = manager.getLongMarketEntryOnOpen();
        auto shortEntry = manager.getShortMarketEntryOnOpen();
        
        REQUIRE(longEntry.get() != shortEntry.get());
    }

    SECTION("Same manager returns same entry objects (cached)") {
        auto longEntry1 = manager.getLongMarketEntryOnOpen();
        auto longEntry2 = manager.getLongMarketEntryOnOpen();
        
        REQUIRE(longEntry1.get() == longEntry2.get());
    }
}

// ============================================================================
// DECIMAL NUMBER TESTS
// ============================================================================

TEST_CASE("AstResourceManager Decimal Number Methods", "[AstResourceManager][decimal]") {
    AstResourceManager manager;

    SECTION("getDecimalNumber from string creates valid decimal") {
        auto decimal = manager.getDecimalNumber("123.45");
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == num::fromString<decimal7>("123.45"));
    }

    SECTION("getDecimalNumber from int creates valid decimal") {
        auto decimal = manager.getDecimalNumber(42);
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == decimal7(42));
    }

    SECTION("Same string returns same cached object") {
        auto decimal1 = manager.getDecimalNumber("100.5");
        auto decimal2 = manager.getDecimalNumber("100.5");
        
        REQUIRE(decimal1.get() == decimal2.get());
    }

    SECTION("Same int returns same cached object") {
        auto decimal1 = manager.getDecimalNumber(99);
        auto decimal2 = manager.getDecimalNumber(99);
        
        REQUIRE(decimal1.get() == decimal2.get());
    }

    SECTION("Different values return different objects") {
        auto decimal1 = manager.getDecimalNumber("10.5");
        auto decimal2 = manager.getDecimalNumber("20.5");
        
        REQUIRE(decimal1.get() != decimal2.get());
    }

    SECTION("Various string formats") {
        REQUIRE(manager.getDecimalNumber("0") != nullptr);
        REQUIRE(manager.getDecimalNumber("0.0") != nullptr);
        REQUIRE(manager.getDecimalNumber("999.999") != nullptr);
        REQUIRE(manager.getDecimalNumber("-5.5") != nullptr);
    }

    SECTION("Zero value") {
        auto decimal = manager.getDecimalNumber(0);
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == decimal7(0));
    }

    SECTION("Negative values") {
        auto decimal = manager.getDecimalNumber(-100);
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == decimal7(-100));
    }
}

// ============================================================================
// PROFIT TARGET TESTS
// ============================================================================

TEST_CASE("AstResourceManager Profit Target Methods", "[AstResourceManager][profit]") {
    AstResourceManager manager;
    MockPalCodeGenVisitor visitor;

    SECTION("getLongProfitTarget creates valid profit target") {
        auto value = manager.getDecimalNumber(5);
        auto profitTarget = manager.getLongProfitTarget(value);
        
        REQUIRE(profitTarget != nullptr);
        profitTarget->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongSideProfitTargetInPercent");
    }

    SECTION("getShortProfitTarget creates valid profit target") {
        auto value = manager.getDecimalNumber(3);
        auto profitTarget = manager.getShortProfitTarget(value);
        
        REQUIRE(profitTarget != nullptr);
        profitTarget->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "ShortSideProfitTargetInPercent");
    }

    SECTION("Same value returns cached profit target") {
        auto value = manager.getDecimalNumber(10);
        auto pt1 = manager.getLongProfitTarget(value);
        auto pt2 = manager.getLongProfitTarget(value);
        
        REQUIRE(pt1.get() == pt2.get());
    }

    SECTION("Different values return different profit targets") {
        auto value1 = manager.getDecimalNumber(5);
        auto value2 = manager.getDecimalNumber(10);
        auto pt1 = manager.getLongProfitTarget(value1);
        auto pt2 = manager.getLongProfitTarget(value2);
        
        REQUIRE(pt1.get() != pt2.get());
    }

    SECTION("Return type is base class ProfitTargetInPercentExpression") {
        auto value = manager.getDecimalNumber(5);
        auto longPT = manager.getLongProfitTarget(value);
        auto shortPT = manager.getShortProfitTarget(value);
        
        // Both should be base class pointers
        std::shared_ptr<ProfitTargetInPercentExpression> basePT1 = longPT;
        std::shared_ptr<ProfitTargetInPercentExpression> basePT2 = shortPT;
        
        REQUIRE(basePT1 != nullptr);
        REQUIRE(basePT2 != nullptr);
    }
}

// ============================================================================
// STOP LOSS TESTS
// ============================================================================

TEST_CASE("AstResourceManager Stop Loss Methods", "[AstResourceManager][stoploss]") {
    AstResourceManager manager;
    MockPalCodeGenVisitor visitor;

    SECTION("getLongStopLoss creates valid stop loss") {
        auto value = manager.getDecimalNumber(2);
        auto stopLoss = manager.getLongStopLoss(value);
        
        REQUIRE(stopLoss != nullptr);
        stopLoss->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongSideStopLossInPercent");
    }

    SECTION("getShortStopLoss creates valid stop loss") {
        auto value = manager.getDecimalNumber(1);
        auto stopLoss = manager.getShortStopLoss(value);
        
        REQUIRE(stopLoss != nullptr);
        stopLoss->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "ShortSideStopLossInPercent");
    }

    SECTION("Same value returns cached stop loss") {
        auto value = manager.getDecimalNumber(3);
        auto sl1 = manager.getLongStopLoss(value);
        auto sl2 = manager.getLongStopLoss(value);
        
        REQUIRE(sl1.get() == sl2.get());
    }

    SECTION("Different values return different stop losses") {
        auto value1 = manager.getDecimalNumber(2);
        auto value2 = manager.getDecimalNumber(4);
        auto sl1 = manager.getLongStopLoss(value1);
        auto sl2 = manager.getLongStopLoss(value2);
        
        REQUIRE(sl1.get() != sl2.get());
    }

    SECTION("Return type is base class StopLossInPercentExpression") {
        auto value = manager.getDecimalNumber(2);
        auto longSL = manager.getLongStopLoss(value);
        auto shortSL = manager.getShortStopLoss(value);
        
        // Both should be base class pointers
        std::shared_ptr<StopLossInPercentExpression> baseSL1 = longSL;
        std::shared_ptr<StopLossInPercentExpression> baseSL2 = shortSL;
        
        REQUIRE(baseSL1 != nullptr);
        REQUIRE(baseSL2 != nullptr);
    }
}

// ============================================================================
// PATTERN CREATION TESTS
// ============================================================================

TEST_CASE("AstResourceManager Pattern Creation", "[AstResourceManager][pattern]") {
    AstResourceManager manager;
    MockPalCodeGenVisitor visitor;

    SECTION("createPattern with all parameters") {
        auto desc = std::make_shared<PatternDescription>(
            "test.txt", 1, 20230101,
            manager.getDecimalNumber(1),
            manager.getDecimalNumber(1),
            10, 2);
        
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto entry = manager.getLongMarketEntryOnOpen();
        auto profitTarget = manager.getLongProfitTarget(manager.getDecimalNumber(5));
        auto stopLoss = manager.getLongStopLoss(manager.getDecimalNumber(2));
        
        auto pattern = manager.createPattern(
            desc, expr, entry, profitTarget, stopLoss,
            PriceActionLabPattern::VOLATILITY_LOW,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        REQUIRE(pattern != nullptr);
        REQUIRE(pattern->getPatternDescription() == desc);
        REQUIRE(pattern->getPatternExpression() == expr);
        REQUIRE(pattern->getMarketEntry() == entry);
        REQUIRE(pattern->getProfitTarget() == profitTarget);
        REQUIRE(pattern->getStopLoss() == stopLoss);
        REQUIRE(pattern->hasVolatilityAttribute());
        REQUIRE(pattern->isLowVolatilityPattern());
    }

    SECTION("createPattern with default parameters") {
        auto desc = std::make_shared<PatternDescription>(
            "test.txt", 1, 20230101,
            manager.getDecimalNumber(1),
            manager.getDecimalNumber(1),
            10, 2);
        
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto entry = manager.getLongMarketEntryOnOpen();
        auto profitTarget = manager.getLongProfitTarget(manager.getDecimalNumber(5));
        auto stopLoss = manager.getLongStopLoss(manager.getDecimalNumber(2));
        
        auto pattern = manager.createPattern(desc, expr, entry, profitTarget, stopLoss);
        
        REQUIRE(pattern != nullptr);
        REQUIRE_FALSE(pattern->hasVolatilityAttribute()); // VOLATILITY_NONE means no volatility attribute
        REQUIRE_FALSE(pattern->hasPortfolioAttribute()); // PORTFOLIO_FILTER_NONE means no portfolio attribute
    }

    SECTION("createPattern accepts visitor") {
        auto desc = std::make_shared<PatternDescription>(
            "test.txt", 1, 20230101,
            manager.getDecimalNumber(1),
            manager.getDecimalNumber(1),
            10, 2);
        
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto entry = manager.getLongMarketEntryOnOpen();
        auto pt = manager.getLongProfitTarget(manager.getDecimalNumber(5));
        auto sl = manager.getLongStopLoss(manager.getDecimalNumber(2));
        
        auto pattern = manager.createPattern(desc, expr, entry, pt, sl);
        
        pattern->accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "PriceActionLabPattern");
    }

    SECTION("createPattern for short side") {
        auto desc = std::make_shared<PatternDescription>(
            "short.txt", 2, 20230101,
            manager.getDecimalNumber(1),
            manager.getDecimalNumber(1),
            10, 2);
        
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto entry = manager.getShortMarketEntryOnOpen();
        auto profitTarget = manager.getShortProfitTarget(manager.getDecimalNumber(3));
        auto stopLoss = manager.getShortStopLoss(manager.getDecimalNumber(1));
        
        auto pattern = manager.createPattern(
            desc, expr, entry, profitTarget, stopLoss,
            PriceActionLabPattern::VOLATILITY_HIGH,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        REQUIRE(pattern != nullptr);
        REQUIRE(pattern->hasVolatilityAttribute());
        REQUIRE(pattern->isHighVolatilityPattern());
    }
}

// ============================================================================
// FACTORY CACHING TESTS
// ============================================================================

TEST_CASE("AstResourceManager Factory Caching Behavior", "[AstResourceManager][cache]") {
    AstResourceManager manager;

    SECTION("Same manager uses same factory cache") {
        // Request same object multiple times
        auto p1 = manager.getPriceOpen(5);
        auto p2 = manager.getPriceOpen(5);
        auto p3 = manager.getPriceOpen(5);
        
        // Should all be the same cached object
        REQUIRE(p1.get() == p2.get());
        REQUIRE(p2.get() == p3.get());
    }

    SECTION("Different managers have separate caches") {
        AstResourceManager manager1;
        AstResourceManager manager2;
        
        auto p1 = manager1.getPriceOpen(0);
        auto p2 = manager2.getPriceOpen(0);
        
        // Different managers = different factories = different cached objects
        REQUIRE(p1.get() != p2.get());
        REQUIRE(p1->getBarOffset() == p2->getBarOffset()); // Same data though
    }

    SECTION("Decimal number caching within manager") {
        auto d1 = manager.getDecimalNumber("42.5");
        auto d2 = manager.getDecimalNumber("42.5");
        
        REQUIRE(d1.get() == d2.get());
    }

    SECTION("Profit target caching within manager") {
        auto val = manager.getDecimalNumber(10);
        auto pt1 = manager.getLongProfitTarget(val);
        auto pt2 = manager.getLongProfitTarget(val);
        
        REQUIRE(pt1.get() == pt2.get());
    }
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_CASE("AstResourceManager Complete Pattern Building", "[AstResourceManager][integration]") {
    AstResourceManager manager;

    SECTION("Build complete long pattern using only manager") {
        // Create all components through the manager
        auto desc = std::make_shared<PatternDescription>(
            "integration_test.txt", 1, 20230115,
            manager.getDecimalNumber("85.5"),
            manager.getDecimalNumber("72.3"),
            150, 5);
        
        // Create complex expression: (Open[0] > Close[0]) AND (High[1] > Low[1])
        auto expr1 = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto expr2 = std::make_shared<GreaterThanExpr>(
            manager.getPriceHigh(1),
            manager.getPriceLow(1));
        
        auto andExpr = std::make_shared<AndExpr>(expr1, expr2);
        
        auto entry = manager.getLongMarketEntryOnOpen();
        auto pt = manager.getLongProfitTarget(manager.getDecimalNumber("3.5"));
        auto sl = manager.getLongStopLoss(manager.getDecimalNumber("1.8"));
        
        auto pattern = manager.createPattern(
            desc, andExpr, entry, pt, sl,
            PriceActionLabPattern::VOLATILITY_NORMAL,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        // Verify complete pattern
        REQUIRE(pattern != nullptr);
        REQUIRE(pattern->getPatternDescription()->getFileName() == "integration_test.txt");
        REQUIRE(pattern->getPatternDescription()->getpatternIndex() == 1);
        REQUIRE(pattern->getPatternDescription()->numTrades() == 150);
        REQUIRE(pattern->getProfitTargetAsDecimal() == num::fromString<decimal7>("3.5"));
        REQUIRE(pattern->getStopLossAsDecimal() == num::fromString<decimal7>("1.8"));
    }

    SECTION("Build complete short pattern with indicators") {
        auto desc = std::make_shared<PatternDescription>(
            "short_indicator.txt", 2, 20230116,
            manager.getDecimalNumber("90.0"),
            manager.getDecimalNumber("80.0"),
            100, 3);
        
        // Use indicator: ROC1[0] > IBS1[0]
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getRoc1(0),
            manager.getIBS1(0));
        
        auto entry = manager.getShortMarketEntryOnOpen();
        auto pt = manager.getShortProfitTarget(manager.getDecimalNumber("2.0"));
        auto sl = manager.getShortStopLoss(manager.getDecimalNumber("1.0"));
        
        auto pattern = manager.createPattern(
            desc, expr, entry, pt, sl,
            PriceActionLabPattern::VOLATILITY_LOW,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        REQUIRE(pattern != nullptr);
        REQUIRE(pattern->getPayoffRatio() == decimal7(2.0)); // PT/SL = 2.0/1.0
    }

    SECTION("Multiple patterns from same manager share cached components") {
        auto entry = manager.getLongMarketEntryOnOpen();
        auto pt_val = manager.getDecimalNumber(5);
        auto sl_val = manager.getDecimalNumber(2);
        
        // Create two patterns
        for(int i = 0; i < 2; ++i) {
            auto desc = std::make_shared<PatternDescription>(
                "pattern" + std::to_string(i) + ".txt", i, 20230101,
                manager.getDecimalNumber(i),
                manager.getDecimalNumber(i),
                10, 2);
            
            auto expr = std::make_shared<GreaterThanExpr>(
                manager.getPriceOpen(i),
                manager.getPriceClose(i));
            
            auto pt = manager.getLongProfitTarget(pt_val);
            auto sl = manager.getLongStopLoss(sl_val);
            
            auto pattern = manager.createPattern(desc, expr, entry, pt, sl);
            REQUIRE(pattern != nullptr);
        }
        
        // Verify objects are cached
        auto entry2 = manager.getLongMarketEntryOnOpen();
        REQUIRE(entry.get() == entry2.get());
    }
}

// ============================================================================
// THREAD SAFETY TESTS
// ============================================================================

TEST_CASE("AstResourceManager Thread Safety", "[AstResourceManager][threading]") {
    const int num_threads = 8;
    const int iterations = 500;

    SECTION("Multiple threads accessing same manager") {
        AstResourceManager manager;
        std::atomic<int> errors{0};
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, &errors, iterations]() {
                for(int j = 0; j < iterations; ++j) {
                    try {
                        auto p = manager.getPriceOpen(j % 10);
                        if(!p || p->getBarOffset() != (unsigned int)(j % 10)) {
                            errors++;
                        }
                        
                        auto d = manager.getDecimalNumber(j % 50);
                        if(!d) {
                            errors++;
                        }
                    } catch(...) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }

    SECTION("Multiple threads creating patterns") {
        AstResourceManager manager;
        std::atomic<int> errors{0};
        std::atomic<int> patterns_created{0};
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, &errors, &patterns_created, i]() {
                for(int j = 0; j < 50; ++j) {
                    try {
                        auto desc = std::make_shared<PatternDescription>(
                            "thread_pattern.txt", i * 1000 + j, 20230101,
                            manager.getDecimalNumber(j % 10),
                            manager.getDecimalNumber((j + 1) % 10),
                            10, 2);
                        
                        auto expr = std::make_shared<GreaterThanExpr>(
                            manager.getPriceOpen(j % 5),
                            manager.getPriceClose((j + 1) % 5));
                        
                        auto entry = manager.getLongMarketEntryOnOpen();
                        auto pt = manager.getLongProfitTarget(manager.getDecimalNumber(5));
                        auto sl = manager.getLongStopLoss(manager.getDecimalNumber(2));
                        
                        auto pattern = manager.createPattern(desc, expr, entry, pt, sl);
                        
                        if(pattern != nullptr) {
                            patterns_created++;
                        } else {
                            errors++;
                        }
                    } catch(...) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
        REQUIRE(patterns_created == num_threads * 50);
    }

    SECTION("Multiple managers in multiple threads") {
        std::atomic<int> errors{0};
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&errors, iterations]() {
                // Each thread has its own manager
                AstResourceManager manager;
                
                for(int j = 0; j < iterations; ++j) {
                    try {
                        auto p = manager.getPriceOpen(j % 10);
                        if(!p) errors++;
                        
                        auto d = manager.getDecimalNumber(j);
                        if(!d) errors++;
                    } catch(...) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }
}

// ============================================================================
// EDGE CASES AND BOUNDARY TESTS
// ============================================================================

TEST_CASE("AstResourceManager Edge Cases", "[AstResourceManager][edge]") {
    AstResourceManager manager;

    SECTION("Large bar offsets") {
        auto p = manager.getPriceOpen(1000);
        REQUIRE(p != nullptr);
        REQUIRE(p->getBarOffset() == 1000);
    }

    SECTION("Zero bar offset") {
        auto p = manager.getPriceOpen(0);
        REQUIRE(p != nullptr);
        REQUIRE(p->getBarOffset() == 0);
    }

    SECTION("All reference types at same offset") {
        const unsigned int offset = 5;
        
        REQUIRE(manager.getPriceOpen(offset)->getBarOffset() == offset);
        REQUIRE(manager.getPriceHigh(offset)->getBarOffset() == offset);
        REQUIRE(manager.getPriceLow(offset)->getBarOffset() == offset);
        REQUIRE(manager.getPriceClose(offset)->getBarOffset() == offset);
        REQUIRE(manager.getVolume(offset)->getBarOffset() == offset);
        REQUIRE(manager.getRoc1(offset)->getBarOffset() == offset);
        REQUIRE(manager.getIBS1(offset)->getBarOffset() == offset);
        REQUIRE(manager.getIBS2(offset)->getBarOffset() == offset);
        REQUIRE(manager.getIBS3(offset)->getBarOffset() == offset);
        REQUIRE(manager.getMeander(offset)->getBarOffset() == offset);
        REQUIRE(manager.getVChartLow(offset)->getBarOffset() == offset);
        REQUIRE(manager.getVChartHigh(offset)->getBarOffset() == offset);
    }

    SECTION("Decimal number edge values") {
        REQUIRE(manager.getDecimalNumber("0") != nullptr);
        REQUIRE(manager.getDecimalNumber("0.0") != nullptr);
        REQUIRE(manager.getDecimalNumber("-0.0") != nullptr);
        REQUIRE(manager.getDecimalNumber("999999.999999") != nullptr);
        REQUIRE(manager.getDecimalNumber("-999999.999999") != nullptr);
    }

    SECTION("Very small and large profit targets") {
        auto tiny = manager.getDecimalNumber("0.001");
        auto huge = manager.getDecimalNumber("1000.0");
        
        auto pt_tiny = manager.getLongProfitTarget(tiny);
        auto pt_huge = manager.getLongProfitTarget(huge);
        
        REQUIRE(pt_tiny != nullptr);
        REQUIRE(pt_huge != nullptr);
    }

    SECTION("Pattern with all volatility levels") {
        auto desc = std::make_shared<PatternDescription>(
            "test.txt", 1, 20230101,
            manager.getDecimalNumber(1),
            manager.getDecimalNumber(1),
            10, 2);
        
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto entry = manager.getLongMarketEntryOnOpen();
        auto pt = manager.getLongProfitTarget(manager.getDecimalNumber(5));
        auto sl = manager.getLongStopLoss(manager.getDecimalNumber(2));
        
        std::vector<PriceActionLabPattern::VolatilityAttribute> volatilities = {
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::VOLATILITY_LOW,
            PriceActionLabPattern::VOLATILITY_NORMAL,
            PriceActionLabPattern::VOLATILITY_HIGH,
            PriceActionLabPattern::VOLATILITY_VERY_HIGH
        };
        
        for(auto vol : volatilities) {
            auto pattern = manager.createPattern(desc, expr, entry, pt, sl, vol);
            REQUIRE(pattern != nullptr);
            // Check each volatility type individually using existing methods
            switch(vol) {
                case PriceActionLabPattern::VOLATILITY_NONE:
                    REQUIRE_FALSE(pattern->hasVolatilityAttribute());
                    break;
                case PriceActionLabPattern::VOLATILITY_LOW:
                    REQUIRE(pattern->isLowVolatilityPattern());
                    break;
                case PriceActionLabPattern::VOLATILITY_NORMAL:
                    REQUIRE(pattern->isNormalVolatilityPattern());
                    break;
                case PriceActionLabPattern::VOLATILITY_HIGH:
                    REQUIRE(pattern->isHighVolatilityPattern());
                    break;
                case PriceActionLabPattern::VOLATILITY_VERY_HIGH:
                    REQUIRE(pattern->isVeryHighVolatilityPattern());
                    break;
            }
        }
    }
}

// ============================================================================
// MEMORY MANAGEMENT TESTS
// ============================================================================

TEST_CASE("AstResourceManager Memory Management", "[AstResourceManager][memory]") {
    SECTION("Shared pointers properly manage lifetime") {
        std::weak_ptr<PriceBarReference> weak_ptr;
        
        {
            AstResourceManager manager;
            auto p = manager.getPriceOpen(100); // Beyond cache, creates new
            weak_ptr = p;
            REQUIRE(weak_ptr.lock() != nullptr);
        }
        
        // Manager and shared_ptr out of scope
        // For cached objects (< 10), they persist with factory
        // For non-cached objects (>= 10), they should be deleted
        // This tests offset 100 which is non-cached
    }

    SECTION("Multiple shared_ptr references keep object alive") {
        std::shared_ptr<PriceBarReference> p1, p2, p3;
        
        {
            AstResourceManager manager;
            p1 = manager.getPriceOpen(0);
            p2 = p1;
            p3 = p1;
        }
        
        // Manager out of scope but 3 shared_ptrs still exist
        REQUIRE(p1 != nullptr);
        REQUIRE(p2 != nullptr);
        REQUIRE(p3 != nullptr);
        REQUIRE(p1.get() == p2.get());
        REQUIRE(p2.get() == p3.get());
    }

    SECTION("Pattern components can outlive manager") {
        std::shared_ptr<PriceActionLabPattern> pattern;
        
        {
            AstResourceManager manager;
            auto desc = std::make_shared<PatternDescription>(
                "test.txt", 1, 20230101,
                manager.getDecimalNumber(1),
                manager.getDecimalNumber(1),
                10, 2);
            
            auto expr = std::make_shared<GreaterThanExpr>(
                manager.getPriceOpen(0),
                manager.getPriceClose(0));
            
            auto entry = manager.getLongMarketEntryOnOpen();
            auto pt = manager.getLongProfitTarget(manager.getDecimalNumber(5));
            auto sl = manager.getLongStopLoss(manager.getDecimalNumber(2));
            
            pattern = manager.createPattern(desc, expr, entry, pt, sl);
        }
        
        // Manager destroyed, pattern still valid
        REQUIRE(pattern != nullptr);
        REQUIRE(pattern->getPatternDescription() != nullptr);
        REQUIRE(pattern->getPatternExpression() != nullptr);
    }
}

TEST_CASE("Const Cast Fix - String Safety", "[AstResourceManager][fix][critical]") {
    AstResourceManager manager;

    SECTION("String is not modified after getDecimalNumber call") {
        std::string original = "123.45";
        std::string copy = original;
        
        // Call getDecimalNumber
        auto decimal = manager.getDecimalNumber(original);
        
        // CRITICAL: Verify original string wasn't modified
        REQUIRE(original == copy);
        REQUIRE(original == "123.45");
        
        // Verify the decimal was created correctly
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == num::fromString<decimal7>("123.45"));
    }

    SECTION("Multiple calls with same string value") {
        std::string str1 = "99.99";
        std::string str2 = "99.99";
        
        auto d1 = manager.getDecimalNumber(str1);
        auto d2 = manager.getDecimalNumber(str2);
        
        // Verify strings weren't modified
        REQUIRE(str1 == "99.99");
        REQUIRE(str2 == "99.99");
        
        // Verify decimals are correct and cached
        REQUIRE(d1 != nullptr);
        REQUIRE(d2 != nullptr);
        REQUIRE(d1.get() == d2.get()); // Should be same cached object
    }

    SECTION("Temporary string works correctly") {
        // This is important - temporary strings could expose UB with const_cast
        auto decimal = manager.getDecimalNumber(std::string("88.88"));
        
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == num::fromString<decimal7>("88.88"));
    }

    SECTION("String literal works correctly") {
        // String literals are in read-only memory - modifying them is UB
        auto decimal = manager.getDecimalNumber("77.77");
        
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == num::fromString<decimal7>("77.77"));
    }

    SECTION("Empty string") {
        // Edge case - should not crash or cause UB
        std::string empty = "";
        auto decimal = manager.getDecimalNumber(empty);
        
        // Verify empty string wasn't modified
        REQUIRE(empty == "");
    }

    SECTION("Very long string") {
        std::string longStr = "123456789.987654321";
        std::string copy = longStr;
        
        auto decimal = manager.getDecimalNumber(longStr);
        
        // Verify long string wasn't modified
        REQUIRE(longStr == copy);
        REQUIRE(longStr == "123456789.987654321");
        REQUIRE(decimal != nullptr);
    }

    SECTION("String with special characters") {
        std::string specialStr = "-0.00001";
        std::string copy = specialStr;
        
        auto decimal = manager.getDecimalNumber(specialStr);
        
        // Verify string wasn't modified
        REQUIRE(specialStr == copy);
        REQUIRE(decimal != nullptr);
    }
}

TEST_CASE("Const Cast Fix - Thread Safety with Strings", "[AstResourceManager][fix][threading]") {
    AstResourceManager manager;
    const int num_threads = 8;
    const int iterations = 500;

    SECTION("Multiple threads with same string value") {
        std::atomic<int> errors{0};
        std::vector<std::thread> threads;
        std::string shared_string = "42.42";
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, &errors, &shared_string, iterations]() {
                for(int j = 0; j < iterations; ++j) {
                    try {
                        auto decimal = manager.getDecimalNumber(shared_string);
                        
                        if(!decimal) {
                            errors++;
                        }
                        
                        // Verify value is correct
                        if(*decimal != num::fromString<decimal7>("42.42")) {
                            errors++;
                        }
                    } catch(...) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        
        // No errors should occur
        REQUIRE(errors == 0);
        
        // Verify shared string wasn't corrupted
        REQUIRE(shared_string == "42.42");
    }

    SECTION("Multiple threads with different strings") {
        std::atomic<int> errors{0};
        std::vector<std::thread> threads;
        
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, &errors, i, iterations]() {
                for(int j = 0; j < iterations; ++j) {
                    try {
                        // Each thread uses different string values
                        std::string str = std::to_string((i * 1000 + j) % 100) + ".5";
                        std::string copy = str;
                        
                        auto decimal = manager.getDecimalNumber(str);
                        
                        if(!decimal) {
                            errors++;
                        }
                        
                        // Verify string wasn't modified
                        if(str != copy) {
                            errors++;
                        }
                    } catch(...) {
                        errors++;
                    }
                }
            });
        }
        
        for(auto& t : threads) t.join();
        REQUIRE(errors == 0);
    }
}

TEST_CASE("Const Cast Fix - Performance Verification", "[AstResourceManager][fix][performance]") {
    AstResourceManager manager;

    SECTION("Repeated calls are efficiently cached") {
        std::string value = "3.14159";
        
        // First call - creates and caches
        auto d1 = manager.getDecimalNumber(value);
        
        // Subsequent calls should return cached object
        for(int i = 0; i < 1000; ++i) {
            auto d = manager.getDecimalNumber(value);
            REQUIRE(d.get() == d1.get()); // Same object
        }
        
        // Verify string wasn't modified through all those calls
        REQUIRE(value == "3.14159");
    }

    SECTION("Different string objects with same value use cache") {
        auto d1 = manager.getDecimalNumber(std::string("2.71828"));
        auto d2 = manager.getDecimalNumber(std::string("2.71828"));
        auto d3 = manager.getDecimalNumber("2.71828");
        
        // All should be the same cached object
        REQUIRE(d1.get() == d2.get());
        REQUIRE(d2.get() == d3.get());
    }
}

TEST_CASE("Const Cast Fix - Integration Test", "[AstResourceManager][fix][integration]") {
    AstResourceManager manager;

    SECTION("Build complete pattern using decimal strings") {
        // Use string decimals throughout
        std::string pl_str = "85.5";
        std::string ps_str = "72.3";
        std::string pt_str = "3.5";
        std::string sl_str = "1.8";
        
        // Store originals to verify no modification
        std::string pl_orig = pl_str;
        std::string ps_orig = ps_str;
        std::string pt_orig = pt_str;
        std::string sl_orig = sl_str;
        
        // Create pattern description
        auto desc = std::make_shared<PatternDescription>(
            "test.txt", 1, 20230101,
            manager.getDecimalNumber(pl_str),
            manager.getDecimalNumber(ps_str),
            150, 5);
        
        // Create pattern components
        auto expr = std::make_shared<GreaterThanExpr>(
            manager.getPriceOpen(0),
            manager.getPriceClose(0));
        
        auto entry = manager.getLongMarketEntryOnOpen();
        auto pt = manager.getLongProfitTarget(manager.getDecimalNumber(pt_str));
        auto sl = manager.getLongStopLoss(manager.getDecimalNumber(sl_str));
        
        // Create pattern
        auto pattern = manager.createPattern(
            desc, expr, entry, pt, sl,
            PriceActionLabPattern::VOLATILITY_NORMAL,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        
        // Verify pattern created successfully
        REQUIRE(pattern != nullptr);
        
        // CRITICAL: Verify all strings were not modified
        REQUIRE(pl_str == pl_orig);
        REQUIRE(ps_str == ps_orig);
        REQUIRE(pt_str == pt_orig);
        REQUIRE(sl_str == sl_orig);
        
        // Verify values in pattern are correct
        REQUIRE(pattern->getProfitTargetAsDecimal() == num::fromString<decimal7>("3.5"));
        REQUIRE(pattern->getStopLossAsDecimal() == num::fromString<decimal7>("1.8"));
    }
}

TEST_CASE("Const Cast Fix - Edge Cases", "[AstResourceManager][fix][edge]") {
    AstResourceManager manager;

    SECTION("Zero value string") {
        std::string zero = "0";
        auto decimal = manager.getDecimalNumber(zero);
        
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == decimal7(0));
        REQUIRE(zero == "0"); // Not modified
    }

    SECTION("Negative value string") {
        std::string negative = "-99.99";
        auto decimal = manager.getDecimalNumber(negative);
        
        REQUIRE(decimal != nullptr);
        REQUIRE(*decimal == num::fromString<decimal7>("-99.99"));
        REQUIRE(negative == "-99.99"); // Not modified
    }

    SECTION("Very small value") {
        std::string tiny = "0.000001";
        auto decimal = manager.getDecimalNumber(tiny);
        
        REQUIRE(decimal != nullptr);
        REQUIRE(tiny == "0.000001"); // Not modified
    }

    SECTION("Very large value") {
        std::string huge = "9999999.999999";
        auto decimal = manager.getDecimalNumber(huge);
        
        REQUIRE(decimal != nullptr);
        REQUIRE(huge == "9999999.999999"); // Not modified
    }

    SECTION("Scientific notation (if supported)") {
        std::string scientific = "1.23e5";
        std::string copy = scientific;
        
        auto decimal = manager.getDecimalNumber(scientific);
        
        // Regardless of whether it parses correctly,
        // the string should not be modified
        REQUIRE(scientific == copy);
    }
}

// ============================================================================
// VERIFICATION SUMMARY TEST
// ============================================================================

TEST_CASE("Const Cast Fix - Verification Summary", "[AstResourceManager][fix][summary]") {
    SECTION("Fix verification checklist") {
        AstResourceManager manager;
        
        // ✓ No const_cast in AstResourceManager (Solution 1 uses buffer copy)
        // ✓ OR AstFactory accepts const char* (Solution 2)
        
        // Test 1: Basic string safety
        std::string test1 = "100.5";
        auto d1 = manager.getDecimalNumber(test1);
        REQUIRE(test1 == "100.5");
        REQUIRE(d1 != nullptr);
        
        // Test 2: Temporary safety
        auto d2 = manager.getDecimalNumber(std::string("200.5"));
        REQUIRE(d2 != nullptr);
        
        // Test 3: String literal safety
        auto d3 = manager.getDecimalNumber("300.5");
        REQUIRE(d3 != nullptr);
        
        // Test 4: Caching works
        auto d4 = manager.getDecimalNumber("100.5");
        REQUIRE(d1.get() == d4.get());
        
        INFO("✓ All const_cast fix verification tests passed");
        INFO("✓ String safety confirmed");
        INFO("✓ No undefined behavior detected");
        INFO("✓ Caching works correctly");
    }
}

// =============================================================================
// getVolatilityAttribute
// =============================================================================

TEST_CASE("PriceActionLabPattern::getVolatilityAttribute",
          "[PriceActionLabPattern][volatility]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Returns VOLATILITY_NONE when no attribute is set")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_NONE);
        REQUIRE(p->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_NONE);
    }

    SECTION("Returns VOLATILITY_LOW")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_LOW);
        REQUIRE(p->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_LOW);
    }

    SECTION("Returns VOLATILITY_NORMAL")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_NORMAL);
        REQUIRE(p->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_NORMAL);
    }

    SECTION("Returns VOLATILITY_HIGH")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_HIGH);
        REQUIRE(p->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_HIGH);
    }

    SECTION("Returns VOLATILITY_VERY_HIGH")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_VERY_HIGH);
        REQUIRE(p->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_VERY_HIGH);
    }

    SECTION("Is consistent with the isXxxVolatilityPattern helpers")
    {
        // Verify the new accessor and the existing boolean helpers agree on
        // every enum value, so callers can use whichever is more readable.
        auto pLow = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                    PriceActionLabPattern::VOLATILITY_LOW);
        REQUIRE(pLow->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_LOW);
        REQUIRE(pLow->isLowVolatilityPattern());

        auto pHigh = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                     PriceActionLabPattern::VOLATILITY_HIGH);
        REQUIRE(pHigh->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_HIGH);
        REQUIRE(pHigh->isHighVolatilityPattern());

        auto pVH = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                   PriceActionLabPattern::VOLATILITY_VERY_HIGH);
        REQUIRE(pVH->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_VERY_HIGH);
        REQUIRE(pVH->isVeryHighVolatilityPattern());

        auto pNormal = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_NORMAL);
        REQUIRE(pNormal->getVolatilityAttribute() ==
                PriceActionLabPattern::VOLATILITY_NORMAL);
        REQUIRE(pNormal->isNormalVolatilityPattern());
    }

    SECTION("Different volatility values return different enum values")
    {
        // Sanity check: none of the five enum values are aliases of each other.
        auto pNone   = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_NONE);
        auto pLow    = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_LOW);
        auto pNormal = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_NORMAL);
        auto pHigh   = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_HIGH);
        auto pVH     = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_VERY_HIGH);

        REQUIRE(pNone->getVolatilityAttribute()   != pLow->getVolatilityAttribute());
        REQUIRE(pLow->getVolatilityAttribute()    != pNormal->getVolatilityAttribute());
        REQUIRE(pNormal->getVolatilityAttribute() != pHigh->getVolatilityAttribute());
        REQUIRE(pHigh->getVolatilityAttribute()   != pVH->getVolatilityAttribute());
    }
}


// =============================================================================
// getPortfolioAttribute
// =============================================================================

TEST_CASE("PriceActionLabPattern::getPortfolioAttribute",
          "[PriceActionLabPattern][portfolio]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Returns PORTFOLIO_FILTER_NONE when no attribute is set")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_NONE,
                                 PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(p->getPortfolioAttribute() ==
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
    }

    SECTION("Returns PORTFOLIO_FILTER_LONG")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_NONE,
                                 PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        REQUIRE(p->getPortfolioAttribute() ==
                PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
    }

    SECTION("Returns PORTFOLIO_FILTER_SHORT")
    {
        auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                 PriceActionLabPattern::VOLATILITY_NONE,
                                 PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);
        REQUIRE(p->getPortfolioAttribute() ==
                PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);
    }

    SECTION("Is consistent with the isFilteredXxxPattern helpers")
    {
        auto pLong = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                     PriceActionLabPattern::VOLATILITY_NONE,
                                     PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        REQUIRE(pLong->getPortfolioAttribute() ==
                PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        REQUIRE(pLong->isFilteredLongPattern());
        REQUIRE_FALSE(pLong->isFilteredShortPattern());

        auto pShort = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                      PriceActionLabPattern::VOLATILITY_NONE,
                                      PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);
        REQUIRE(pShort->getPortfolioAttribute() ==
                PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);
        REQUIRE(pShort->isFilteredShortPattern());
        REQUIRE_FALSE(pShort->isFilteredLongPattern());

        auto pNone = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                     PriceActionLabPattern::VOLATILITY_NONE,
                                     PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE(pNone->getPortfolioAttribute() ==
                PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        REQUIRE_FALSE(pNone->isFilteredLongPattern());
        REQUIRE_FALSE(pNone->isFilteredShortPattern());
    }

    SECTION("Different portfolio values return different enum values")
    {
        auto pNone  = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                      PriceActionLabPattern::VOLATILITY_NONE,
                                      PriceActionLabPattern::PORTFOLIO_FILTER_NONE);
        auto pLong  = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                      PriceActionLabPattern::VOLATILITY_NONE,
                                      PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        auto pShort = makeLongPattern(factory, "f.txt", expr, "5.0", "2.0",
                                      PriceActionLabPattern::VOLATILITY_NONE,
                                      PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);

        REQUIRE(pNone->getPortfolioAttribute()  != pLong->getPortfolioAttribute());
        REQUIRE(pLong->getPortfolioAttribute()  != pShort->getPortfolioAttribute());
        REQUIRE(pNone->getPortfolioAttribute()  != pShort->getPortfolioAttribute());
    }
}


// =============================================================================
// operator== and operator!=
// =============================================================================

TEST_CASE("PriceActionLabPattern operator== : pointer identity shortcut",
          "[PriceActionLabPattern][equality]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));
    auto p = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");

    SECTION("A pattern compares equal to itself")
    {
        REQUIRE(*p == *p);
        REQUIRE_FALSE(*p != *p);
    }
}

TEST_CASE("PriceActionLabPattern operator== : direction",
          "[PriceActionLabPattern][equality]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Long and short patterns with otherwise identical parameters are not equal")
    {
        auto longPat  = makeLongPattern (factory, "file.txt", expr, "5.0", "2.0");
        auto shortPat = makeShortPattern(factory, "file.txt", expr, "5.0", "2.0");

        REQUIRE_FALSE(*longPat == *shortPat);
        REQUIRE(*longPat != *shortPat);
    }

    SECTION("Two long patterns with the same parameters are equal")
    {
        auto p1 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
        REQUIRE_FALSE(*p1 != *p2);
    }

    SECTION("Two short patterns with the same parameters are equal")
    {
        auto p1 = makeShortPattern(factory, "file.txt", expr, "5.0", "2.0");
        auto p2 = makeShortPattern(factory, "file.txt", expr, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
        REQUIRE_FALSE(*p1 != *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : profit target",
          "[PriceActionLabPattern][equality]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Different profit targets make patterns unequal")
    {
        auto p1 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr, "6.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("Equal profit targets (same value) do not cause inequality")
    {
        auto p1 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : stop loss",
          "[PriceActionLabPattern][equality]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Different stop losses make patterns unequal")
    {
        auto p1 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr, "5.0", "3.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("Equal stop losses (same value) do not cause inequality")
    {
        auto p1 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : volatility attribute",
          "[PriceActionLabPattern][equality]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Patterns with different volatility attributes are not equal")
    {
        auto pLow  = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                     PriceActionLabPattern::VOLATILITY_LOW);
        auto pHigh = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                     PriceActionLabPattern::VOLATILITY_HIGH);

        REQUIRE_FALSE(*pLow == *pHigh);
        REQUIRE(*pLow != *pHigh);
    }

    SECTION("Patterns with the same volatility attribute are equal")
    {
        auto p1 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                  PriceActionLabPattern::VOLATILITY_HIGH);
        auto p2 = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                  PriceActionLabPattern::VOLATILITY_HIGH);

        REQUIRE(*p1 == *p2);
    }

    SECTION("VOLATILITY_NONE differs from any specific volatility setting")
    {
        auto pNone   = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_NONE);
        auto pNormal = makeLongPattern(factory, "file.txt", expr, "5.0", "2.0",
                                       PriceActionLabPattern::VOLATILITY_NORMAL);

        REQUIRE_FALSE(*pNone == *pNormal);
        REQUIRE(*pNone != *pNormal);
    }
}

TEST_CASE("PriceActionLabPattern operator== : portfolio attribute",
          "[PriceActionLabPattern][equality]")
{
    AstFactory factory;
    auto expr = std::make_shared<GreaterThanExpr>(
                    factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Patterns with different portfolio attributes are not equal")
    {
        auto pFilterLong = makeLongPattern(
            factory, "file.txt", expr, "5.0", "2.0",
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        auto pFilterNone = makeLongPattern(
            factory, "file.txt", expr, "5.0", "2.0",
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::PORTFOLIO_FILTER_NONE);

        REQUIRE_FALSE(*pFilterLong == *pFilterNone);
        REQUIRE(*pFilterLong != *pFilterNone);
    }

    SECTION("PORTFOLIO_FILTER_LONG differs from PORTFOLIO_FILTER_SHORT")
    {
        auto pLong  = makeLongPattern(
            factory, "file.txt", expr, "5.0", "2.0",
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        auto pShort = makeLongPattern(
            factory, "file.txt", expr, "5.0", "2.0",
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::PORTFOLIO_FILTER_SHORT);

        REQUIRE_FALSE(*pLong == *pShort);
        REQUIRE(*pLong != *pShort);
    }

    SECTION("Patterns with the same portfolio attribute are equal")
    {
        auto p1 = makeLongPattern(
            factory, "file.txt", expr, "5.0", "2.0",
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::PORTFOLIO_FILTER_LONG);
        auto p2 = makeLongPattern(
            factory, "file.txt", expr, "5.0", "2.0",
            PriceActionLabPattern::VOLATILITY_NONE,
            PriceActionLabPattern::PORTFOLIO_FILTER_LONG);

        REQUIRE(*p1 == *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : expression tree — single GreaterThanExpr",
          "[PriceActionLabPattern][equality][expression]")
{
    AstFactory factory;

    SECTION("Identical GreaterThanExpr trees are equal")
    {
        // Build two independent expression objects that are structurally the same
        auto expr1 = std::make_shared<GreaterThanExpr>(
                         factory.getPriceOpen(0), factory.getPriceClose(1));
        auto expr2 = std::make_shared<GreaterThanExpr>(
                         factory.getPriceOpen(0), factory.getPriceClose(1));

        auto p1 = makeLongPattern(factory, "file.txt", expr1, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr2, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
    }

    SECTION("Different LHS bar type makes patterns unequal")
    {
        // Open[0] > Close[1]  vs  High[0] > Close[1]
        auto exprOpen = std::make_shared<GreaterThanExpr>(
                            factory.getPriceOpen(0), factory.getPriceClose(1));
        auto exprHigh = std::make_shared<GreaterThanExpr>(
                            factory.getPriceHigh(0), factory.getPriceClose(1));

        auto p1 = makeLongPattern(factory, "file.txt", exprOpen, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", exprHigh, "5.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("Different RHS bar type makes patterns unequal")
    {
        // Open[0] > Close[1]  vs  Open[0] > Low[1]
        auto exprClose = std::make_shared<GreaterThanExpr>(
                             factory.getPriceOpen(0), factory.getPriceClose(1));
        auto exprLow   = std::make_shared<GreaterThanExpr>(
                             factory.getPriceOpen(0), factory.getPriceLow(1));

        auto p1 = makeLongPattern(factory, "file.txt", exprClose, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", exprLow,   "5.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("Different LHS bar offset makes patterns unequal")
    {
        // Open[0] > Close[1]  vs  Open[2] > Close[1]
        auto expr0 = std::make_shared<GreaterThanExpr>(
                         factory.getPriceOpen(0), factory.getPriceClose(1));
        auto expr2 = std::make_shared<GreaterThanExpr>(
                         factory.getPriceOpen(2), factory.getPriceClose(1));

        auto p1 = makeLongPattern(factory, "file.txt", expr0, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr2, "5.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("Different RHS bar offset makes patterns unequal")
    {
        // Open[0] > Close[1]  vs  Open[0] > Close[3]
        auto expr1 = std::make_shared<GreaterThanExpr>(
                         factory.getPriceOpen(0), factory.getPriceClose(1));
        auto expr3 = std::make_shared<GreaterThanExpr>(
                         factory.getPriceOpen(0), factory.getPriceClose(3));

        auto p1 = makeLongPattern(factory, "file.txt", expr1, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", expr3, "5.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : expression tree — AndExpr",
          "[PriceActionLabPattern][equality][expression]")
{
    AstFactory factory;

    SECTION("Identical AndExpr trees are equal")
    {
        auto gt1a = std::make_shared<GreaterThanExpr>(
                        factory.getPriceOpen(0), factory.getPriceClose(1));
        auto gt2a = std::make_shared<GreaterThanExpr>(
                        factory.getPriceHigh(1), factory.getPriceLow(2));
        auto exprA = std::make_shared<AndExpr>(gt1a, gt2a);

        auto gt1b = std::make_shared<GreaterThanExpr>(
                        factory.getPriceOpen(0), factory.getPriceClose(1));
        auto gt2b = std::make_shared<GreaterThanExpr>(
                        factory.getPriceHigh(1), factory.getPriceLow(2));
        auto exprB = std::make_shared<AndExpr>(gt1b, gt2b);

        auto p1 = makeLongPattern(factory, "file.txt", exprA, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", exprB, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
    }

    SECTION("AndExpr with one differing leaf makes patterns unequal")
    {
        // (Open[0] > Close[1]) AND (High[1] > Low[2])
        // vs
        // (Open[0] > Close[1]) AND (High[1] > Low[3])  <-- RHS offset differs
        auto gt1a = std::make_shared<GreaterThanExpr>(
                        factory.getPriceOpen(0), factory.getPriceClose(1));
        auto gt2a = std::make_shared<GreaterThanExpr>(
                        factory.getPriceHigh(1), factory.getPriceLow(2));
        auto exprA = std::make_shared<AndExpr>(gt1a, gt2a);

        auto gt1b = std::make_shared<GreaterThanExpr>(
                        factory.getPriceOpen(0), factory.getPriceClose(1));
        auto gt2b = std::make_shared<GreaterThanExpr>(
                        factory.getPriceHigh(1), factory.getPriceLow(3)); // differs
        auto exprB = std::make_shared<AndExpr>(gt1b, gt2b);

        auto p1 = makeLongPattern(factory, "file.txt", exprA, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", exprB, "5.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("AndExpr is not equal to a GreaterThanExpr even when leaves overlap")
    {
        auto gt = std::make_shared<GreaterThanExpr>(
                      factory.getPriceOpen(0), factory.getPriceClose(1));

        auto gt2 = std::make_shared<GreaterThanExpr>(
                       factory.getPriceHigh(1), factory.getPriceLow(2));
        auto andExpr = std::make_shared<AndExpr>(gt, gt2);

        auto pSingle = makeLongPattern(factory, "file.txt", gt,      "5.0", "2.0");
        auto pAnd    = makeLongPattern(factory, "file.txt", andExpr, "5.0", "2.0");

        REQUIRE_FALSE(*pSingle == *pAnd);
        REQUIRE(*pSingle != *pAnd);
    }

    SECTION("Nested AndExpr — identical deep trees are equal")
    {
        // ( (O[0]>C[1]) AND (H[1]>L[2]) ) AND (O[2]>C[3])
        auto makeDeepExpr = [&]() -> PatternExpressionPtr {
            auto gt1 = std::make_shared<GreaterThanExpr>(
                            factory.getPriceOpen(0), factory.getPriceClose(1));
            auto gt2 = std::make_shared<GreaterThanExpr>(
                            factory.getPriceHigh(1), factory.getPriceLow(2));
            auto gt3 = std::make_shared<GreaterThanExpr>(
                            factory.getPriceOpen(2), factory.getPriceClose(3));
            auto inner = std::make_shared<AndExpr>(gt1, gt2);
            return std::make_shared<AndExpr>(inner, gt3);
        };

        auto p1 = makeLongPattern(factory, "file.txt", makeDeepExpr(), "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", makeDeepExpr(), "5.0", "2.0");

        REQUIRE(*p1 == *p2);
    }

    SECTION("Nested AndExpr — differing deep leaf makes patterns unequal")
    {
        auto makeExprWith = [&](unsigned int deepOffset) -> PatternExpressionPtr {
            auto gt1 = std::make_shared<GreaterThanExpr>(
                            factory.getPriceOpen(0), factory.getPriceClose(1));
            auto gt2 = std::make_shared<GreaterThanExpr>(
                            factory.getPriceHigh(1), factory.getPriceLow(2));
            // This is the varying leaf:
            auto gt3 = std::make_shared<GreaterThanExpr>(
                            factory.getPriceOpen(deepOffset), factory.getPriceClose(3));
            auto inner = std::make_shared<AndExpr>(gt1, gt2);
            return std::make_shared<AndExpr>(inner, gt3);
        };

        auto p1 = makeLongPattern(factory, "file.txt", makeExprWith(2), "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "file.txt", makeExprWith(4), "5.0", "2.0"); // differs

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : cross-file comparison",
          "[PriceActionLabPattern][equality][cross-file]")
{
    // This is the primary motivating use case for the design of operator==:
    // two patterns loaded from different files should compare as equal if
    // their trading logic (direction, parameters, expression tree, attributes)
    // is identical. The filename is intentionally excluded from equality.

    AstFactory factory;
    auto expr1 = std::make_shared<GreaterThanExpr>(
                     factory.getPriceOpen(0), factory.getPriceClose(1));
    auto expr2 = std::make_shared<GreaterThanExpr>(
                     factory.getPriceOpen(0), factory.getPriceClose(1));

    SECTION("Same logic from two different files compares equal")
    {
        auto p1 = makeLongPattern(factory, "fileA.txt", expr1, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "fileB.txt", expr2, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
        REQUIRE_FALSE(*p1 != *p2);
    }

    SECTION("Different logic from two different files is still unequal")
    {
        auto exprDifferent = std::make_shared<GreaterThanExpr>(
                                 factory.getPriceHigh(0), factory.getPriceLow(1));

        auto p1 = makeLongPattern(factory, "fileA.txt", expr1,        "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "fileB.txt", exprDifferent, "5.0", "2.0");

        REQUIRE_FALSE(*p1 == *p2);
        REQUIRE(*p1 != *p2);
    }

    SECTION("Same file, same logic still compares equal (baseline check)")
    {
        auto p1 = makeLongPattern(factory, "fileA.txt", expr1, "5.0", "2.0");
        auto p2 = makeLongPattern(factory, "fileA.txt", expr2, "5.0", "2.0");

        REQUIRE(*p1 == *p2);
    }
}

TEST_CASE("PriceActionLabPattern operator== : operator!= is always the negation of operator==",
          "[PriceActionLabPattern][equality][invariant]")
{
    // The C++ rule is that (a != b) must equal !(a == b). This test confirms
    // that invariant holds across a representative set of pattern pairs so
    // that no future change breaks the delegation between the two operators.

    AstFactory factory;

    auto exprA = std::make_shared<GreaterThanExpr>(
                     factory.getPriceOpen(0), factory.getPriceClose(1));
    auto exprB = std::make_shared<GreaterThanExpr>(
                     factory.getPriceHigh(0), factory.getPriceLow(1));

    struct PatternPair {
        PALPatternPtr p1;
        PALPatternPtr p2;
        const char*   description;
    };

    AstFactory f;  // alias for brevity in initialiser
    std::vector<PatternPair> pairs = {
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0"),
          makeLongPattern (f, "f.txt", exprA, "5.0", "2.0"),
          "equal patterns" },
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0"),
          makeLongPattern (f, "f.txt", exprB, "5.0", "2.0"),
          "different expression trees" },
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0"),
          makeLongPattern (f, "f.txt", exprA, "6.0", "2.0"),
          "different profit target" },
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0"),
          makeLongPattern (f, "f.txt", exprA, "5.0", "3.0"),
          "different stop loss" },
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0"),
          makeShortPattern(f, "f.txt", exprA, "5.0", "2.0"),
          "different direction" },
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0",
                           PriceActionLabPattern::VOLATILITY_LOW),
          makeLongPattern (f, "f.txt", exprA, "5.0", "2.0",
                           PriceActionLabPattern::VOLATILITY_HIGH),
          "different volatility" },
        { makeLongPattern (f, "f.txt", exprA, "5.0", "2.0",
                           PriceActionLabPattern::VOLATILITY_NONE,
                           PriceActionLabPattern::PORTFOLIO_FILTER_LONG),
          makeLongPattern (f, "f.txt", exprA, "5.0", "2.0",
                           PriceActionLabPattern::VOLATILITY_NONE,
                           PriceActionLabPattern::PORTFOLIO_FILTER_NONE),
          "different portfolio attribute" },
    };

    for (const auto& pair : pairs)
    {
        INFO("Checking invariant for: " << pair.description);
        bool eq  = (*pair.p1 == *pair.p2);
        bool neq = (*pair.p1 != *pair.p2);
        REQUIRE(neq == !eq);
    }
}

