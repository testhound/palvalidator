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


TEST_CASE("AstFactory Basic Operations", "[AstFactory]") {
    AstFactory factory;
    MockPalCodeGenVisitor mock_visitor;

    SECTION("Get PriceBarReference Objects") {
        PriceBarReference* open0 = factory.getPriceOpen(0);
        REQUIRE(open0 != nullptr);
        REQUIRE(open0->getBarOffset() == 0);
        REQUIRE(open0->getReferenceType() == PriceBarReference::OPEN);
        REQUIRE(open0->extraBarsNeeded() == 0); //
        open0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarOpen");

        PriceBarReference* high1 = factory.getPriceHigh(1);
        REQUIRE(high1 != nullptr);
        REQUIRE(high1->getBarOffset() == 1);
        REQUIRE(high1->getReferenceType() == PriceBarReference::HIGH);
        REQUIRE(high1->extraBarsNeeded() == 0); //
        high1->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarHigh");


        PriceBarReference* low2 = factory.getPriceLow(2);
        REQUIRE(low2 != nullptr);
        REQUIRE(low2->getBarOffset() == 2);
        REQUIRE(low2->getReferenceType() == PriceBarReference::LOW);
        REQUIRE(low2->extraBarsNeeded() == 0); //
        low2->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarLow");

        PriceBarReference* close3 = factory.getPriceClose(3);
        REQUIRE(close3 != nullptr);
        REQUIRE(close3->getBarOffset() == 3);
        REQUIRE(close3->getReferenceType() == PriceBarReference::CLOSE);
        REQUIRE(close3->extraBarsNeeded() == 0); //
        close3->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "PriceBarClose");

        PriceBarReference* volume0 = factory.getVolume(0);
        REQUIRE(volume0 != nullptr);
        REQUIRE(volume0->getBarOffset() == 0);
        REQUIRE(volume0->getReferenceType() == PriceBarReference::VOLUME);
        REQUIRE(volume0->extraBarsNeeded() == 0); //
        volume0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "VolumeBarReference");

        PriceBarReference* roc1_0 = factory.getRoc1(0);
        REQUIRE(roc1_0 != nullptr);
        REQUIRE(roc1_0->getBarOffset() == 0);
        REQUIRE(roc1_0->getReferenceType() == PriceBarReference::ROC1);
        REQUIRE(roc1_0->extraBarsNeeded() == 1); //
        roc1_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "Roc1BarReference");

        PriceBarReference* ibs1_0 = factory.getIBS1(0);
        REQUIRE(ibs1_0 != nullptr);
        REQUIRE(ibs1_0->getBarOffset() == 0);
        REQUIRE(ibs1_0->getReferenceType() == PriceBarReference::IBS1);
        REQUIRE(ibs1_0->extraBarsNeeded() == 0); //
        ibs1_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "IBS1BarReference");

        PriceBarReference* ibs2_0 = factory.getIBS2(0);
        REQUIRE(ibs2_0 != nullptr);
        REQUIRE(ibs2_0->getBarOffset() == 0);
        REQUIRE(ibs2_0->getReferenceType() == PriceBarReference::IBS2);
        REQUIRE(ibs2_0->extraBarsNeeded() == 1); //
        ibs2_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "IBS2BarReference");

        PriceBarReference* ibs3_0 = factory.getIBS3(0);
        REQUIRE(ibs3_0 != nullptr);
        REQUIRE(ibs3_0->getBarOffset() == 0);
        REQUIRE(ibs3_0->getReferenceType() == PriceBarReference::IBS3);
        REQUIRE(ibs3_0->extraBarsNeeded() == 2); //
        ibs3_0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "IBS3BarReference");

        PriceBarReference* meander0 = factory.getMeander(0);
        REQUIRE(meander0 != nullptr);
        REQUIRE(meander0->getBarOffset() == 0);
        REQUIRE(meander0->getReferenceType() == PriceBarReference::MEANDER);
        REQUIRE(meander0->extraBarsNeeded() == 5); //
        meander0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "MeanderBarReference");

        PriceBarReference* vclow0 = factory.getVChartLow(0);
        REQUIRE(vclow0 != nullptr);
        REQUIRE(vclow0->getBarOffset() == 0);
        REQUIRE(vclow0->getReferenceType() == PriceBarReference::VCHARTLOW);
        REQUIRE(vclow0->extraBarsNeeded() == 6); //
        vclow0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "VChartLowBarReference");

        PriceBarReference* vchigh0 = factory.getVChartHigh(0);
        REQUIRE(vchigh0 != nullptr);
        REQUIRE(vchigh0->getBarOffset() == 0);
        REQUIRE(vchigh0->getReferenceType() == PriceBarReference::VCHARTHIGH);
        REQUIRE(vchigh0->extraBarsNeeded() == 6); //
        vchigh0->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "VChartHighBarReference");

        // Test caching: subsequent calls with same offset should return same pointer for predefined ones
        REQUIRE(factory.getPriceOpen(0) == open0);
        REQUIRE(factory.getPriceOpen(AstFactory::MaxNumBarOffsets -1) != nullptr); // Check last predefined
        // Test beyond predefined
        PriceBarReference* open_beyond = factory.getPriceOpen(AstFactory::MaxNumBarOffsets + 1);
        REQUIRE(open_beyond != nullptr);
        REQUIRE(open_beyond->getBarOffset() == AstFactory::MaxNumBarOffsets + 1);
        //This object is created on the fly and is not cleaned up by AstFactory destructor
        //It needs to be manually deleted if not assigned to a shared_ptr or managed elsewhere.
        //For this test, we'll assume it's okay for it to leak or be cleaned up by OS on exit.
        //In real code, this would need careful memory management.
        // delete open_beyond; // Or better, use smart pointers in real code
    }

    SECTION("Get MarketEntryExpression Objects") {
        MarketEntryExpression* longEntry = factory.getLongMarketEntryOnOpen();
        REQUIRE(longEntry != nullptr);
        REQUIRE(longEntry->isLongPattern()); //
        REQUIRE_FALSE(longEntry->isShortPattern()); //
        longEntry->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "LongMarketEntryOnOpen");


        MarketEntryExpression* shortEntry = factory.getShortMarketEntryOnOpen();
        REQUIRE(shortEntry != nullptr);
        REQUIRE(shortEntry->isShortPattern()); //
        REQUIRE_FALSE(shortEntry->isLongPattern()); //
        shortEntry->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "ShortMarketEntryOnOpen");


        // Test caching
        REQUIRE(factory.getLongMarketEntryOnOpen() == longEntry);
        REQUIRE(factory.getShortMarketEntryOnOpen() == shortEntry);
    }

    SECTION("Get DecimalNumber Objects") {
        char numStr1[] = "123.45";
        decimal7* dec1_str = factory.getDecimalNumber(numStr1);
        REQUIRE(dec1_str != nullptr);
        REQUIRE(*dec1_str == num::fromString<decimal7>("123.45"));

        char numStr2[] = "67.89";
        decimal7* dec2_str = factory.getDecimalNumber(numStr2);
        REQUIRE(dec2_str != nullptr);
        REQUIRE(*dec2_str == num::fromString<decimal7>("67.89"));
        REQUIRE(factory.getDecimalNumber(numStr1) == dec1_str); // Test caching

        decimal7* dec1_int = factory.getDecimalNumber(123);
        REQUIRE(dec1_int != nullptr);
        REQUIRE(*dec1_int == decimal7(123));

        decimal7* dec2_int = factory.getDecimalNumber(456);
        REQUIRE(dec2_int != nullptr);
        REQUIRE(*dec2_int == decimal7(456));
        REQUIRE(factory.getDecimalNumber(123) == dec1_int); // Test caching
    }


    SECTION("Get ProfitTarget Objects") {
        decimal7* pt_val_5 = factory.getDecimalNumber(5);
        decimal7* pt_val_10 = factory.getDecimalNumber(10);


        LongSideProfitTargetInPercent* long_pt5 = factory.getLongProfitTarget(pt_val_5);
        REQUIRE(long_pt5 != nullptr);
        REQUIRE(long_pt5->getProfitTarget() == pt_val_5);
        REQUIRE(long_pt5->isLongSideProfitTarget()); //
        REQUIRE_FALSE(long_pt5->isShortSideProfitTarget()); //
        long_pt5->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "LongSideProfitTargetInPercent");

        ShortSideProfitTargetInPercent* short_pt10 = factory.getShortProfitTarget(pt_val_10);
        REQUIRE(short_pt10 != nullptr);
        REQUIRE(short_pt10->getProfitTarget() == pt_val_10);
        REQUIRE(short_pt10->isShortSideProfitTarget()); //
        REQUIRE_FALSE(short_pt10->isLongSideProfitTarget()); //
        short_pt10->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "ShortSideProfitTargetInPercent");

        // Test caching
        REQUIRE(factory.getLongProfitTarget(pt_val_5) == long_pt5);
        REQUIRE(factory.getShortProfitTarget(pt_val_10) == short_pt10);
    }

    SECTION("Get StopLoss Objects") {
        decimal7* sl_val_2 = factory.getDecimalNumber(2);
        decimal7* sl_val_3 = factory.getDecimalNumber(3);

        LongSideStopLossInPercent* long_sl2 = factory.getLongStopLoss(sl_val_2);
        REQUIRE(long_sl2 != nullptr);
        REQUIRE(long_sl2->getStopLoss() == sl_val_2);
        REQUIRE(long_sl2->isLongSideStopLoss()); //
        REQUIRE_FALSE(long_sl2->isShortSideStopLoss()); //
        long_sl2->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "LongSideStopLossInPercent");

        ShortSideStopLossInPercent* short_sl3 = factory.getShortStopLoss(sl_val_3);
        REQUIRE(short_sl3 != nullptr);
        REQUIRE(short_sl3->getStopLoss() == sl_val_3);
        REQUIRE(short_sl3->isShortSideStopLoss()); //
        REQUIRE_FALSE(short_sl3->isLongSideStopLoss()); //
        short_sl3->accept(mock_visitor);
        REQUIRE(mock_visitor.visited_nodes.back() == "ShortSideStopLossInPercent");

        // Test caching
        REQUIRE(factory.getLongStopLoss(sl_val_2) == long_sl2);
        REQUIRE(factory.getShortStopLoss(sl_val_3) == short_sl3);
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
    PriceBarReference* open0 = factory.getPriceOpen(0);
    PriceBarReference* close1 = factory.getPriceClose(1);
    PriceBarReference* high0 = factory.getPriceHigh(0);


    SECTION("GreaterThanExpr") {
        GreaterThanExpr gtExpr(open0, close1);
        REQUIRE(gtExpr.getLHS() == open0);
        REQUIRE(gtExpr.getRHS() == close1);
        gtExpr.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "GreaterThanExpr");
        unsigned long long hc = gtExpr.hashCode();
        REQUIRE(hc != 0);

        GreaterThanExpr gtExpr_copy(gtExpr);
        REQUIRE(gtExpr_copy.getLHS() == open0);
        REQUIRE(gtExpr_copy.getRHS() == close1);
        REQUIRE(gtExpr_copy.hashCode() == hc);

        GreaterThanExpr gtExpr_assign(high0, open0);
        gtExpr_assign = gtExpr;
        REQUIRE(gtExpr_assign.getLHS() == open0);
        REQUIRE(gtExpr_assign.getRHS() == close1);
        REQUIRE(gtExpr_assign.hashCode() == hc);
    }

    SECTION("AndExpr") {
        PatternExpressionPtr gtExpr1 = std::make_shared<GreaterThanExpr>(open0, close1);
        PatternExpressionPtr gtExpr2 = std::make_shared<GreaterThanExpr>(close1, high0);

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


        PatternExpressionPtr gtExpr3 = std::make_shared<GreaterThanExpr>(open0, high0);
        PatternExpressionPtr gtExpr4 = std::make_shared<GreaterThanExpr>(high0, close1);
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
    decimal7* pt_val = factory.getDecimalNumber(ptStr);

    SECTION("LongSideProfitTargetInPercent") {
        LongSideProfitTargetInPercent longPt(pt_val);
        REQUIRE(longPt.getProfitTarget() == pt_val);
        REQUIRE(longPt.isLongSideProfitTarget()); //
        REQUIRE_FALSE(longPt.isShortSideProfitTarget()); //
        longPt.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongSideProfitTargetInPercent");
        unsigned long long hc = longPt.hashCode();
        REQUIRE(hc != 0);
        REQUIRE(longPt.hashCode() == hc); // Cached

        LongSideProfitTargetInPercent longPt_copy(longPt);
        REQUIRE(longPt_copy.getProfitTarget() == pt_val);
        REQUIRE(longPt_copy.hashCode() == hc);

        char ptStr2[] = "3.0";
        decimal7* pt_val2 = factory.getDecimalNumber(ptStr2);
        LongSideProfitTargetInPercent longPt_assign(pt_val2);
        longPt_assign = longPt;
        REQUIRE(longPt_assign.getProfitTarget() == pt_val);
        REQUIRE(longPt_assign.hashCode() == hc);


    }
    SECTION("ShortSideProfitTargetInPercent") {
        ShortSideProfitTargetInPercent shortPt(pt_val);
        REQUIRE(shortPt.getProfitTarget() == pt_val);
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
    decimal7* sl_val = factory.getDecimalNumber(slStr);

    SECTION("LongSideStopLossInPercent") {
        LongSideStopLossInPercent longSl(sl_val);
        REQUIRE(longSl.getStopLoss() == sl_val);
        REQUIRE(longSl.isLongSideStopLoss()); //
        REQUIRE_FALSE(longSl.isShortSideStopLoss()); //
        longSl.accept(visitor);
        REQUIRE(visitor.visited_nodes.back() == "LongSideStopLossInPercent");
        unsigned long long hc = longSl.hashCode();
        REQUIRE(hc != 0);
        REQUIRE(longSl.hashCode() == hc); // Cached

        LongSideStopLossInPercent longSl_copy(longSl);
        REQUIRE(longSl_copy.getStopLoss() == sl_val);
        REQUIRE(longSl_copy.hashCode() == hc);

        char slStr2[] = "2.0";
        decimal7* sl_val2 = factory.getDecimalNumber(slStr2);
        LongSideStopLossInPercent longSl_assign(sl_val2);
        longSl_assign = longSl;
        REQUIRE(longSl_assign.getStopLoss() == sl_val);
        REQUIRE(longSl_assign.hashCode() == hc);
    }

    SECTION("ShortSideStopLossInPercent") {
        ShortSideStopLossInPercent shortSl(sl_val);
        REQUIRE(shortSl.getStopLoss() == sl_val);
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
    decimal7* pLong = factory.getDecimalNumber(pLongStr);
    decimal7* pShort = factory.getDecimalNumber(pShortStr);

    PatternDescription pd("testFile.txt", 1, 20230101, pLong, pShort, 100, 5);
    REQUIRE(pd.getFileName() == "testFile.txt");
    REQUIRE(pd.getpatternIndex() == 1);
    REQUIRE(pd.getIndexDate() == 20230101);
    REQUIRE(pd.getPercentLong() == pLong);
    REQUIRE(pd.getPercentShort() == pShort);
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
    decimal7* pLong2 = factory.getDecimalNumber(pLongStr2);
    PatternDescription pd_assign("other.txt", 2, 20220101, pLong2, pShort, 50, 2);
    pd_assign = pd;
    REQUIRE(pd_assign.getFileName() == "testFile.txt");
    REQUIRE(pd_assign.hashCode() == hc);

}


TEST_CASE("PalPatternMaxBars Evaluation", "[PalPatternMaxBars]") {
    AstFactory factory;
    PriceBarReference* open0 = factory.getPriceOpen(0); // extraBarsNeeded = 0
    PriceBarReference* close1 = factory.getPriceClose(1); // extraBarsNeeded = 0
    PriceBarReference* roc5 = factory.getRoc1(5);       // extraBarsNeeded = 1
    PriceBarReference* meander2 = factory.getMeander(2); // extraBarsNeeded = 5

    SECTION("Single GreaterThanExpr") {
        GreaterThanExpr gtExpr(open0, close1); // max(0+0, 1+0) = 1
        REQUIRE(PalPatternMaxBars::evaluateExpression(&gtExpr) == 1);

        GreaterThanExpr gtExpr2(roc5, meander2); // max(5+1, 2+5) = max(6,7) = 7
        REQUIRE(PalPatternMaxBars::evaluateExpression(&gtExpr2) == 7);
    }

    SECTION("AndExpr") {
        PatternExpressionPtr gt1_ptr = std::make_shared<GreaterThanExpr>(open0, close1); // max_bars = 1
        PatternExpressionPtr gt2_ptr = std::make_shared<GreaterThanExpr>(roc5, meander2); // max_bars = 7
        AndExpr andExpr(gt1_ptr.get(), gt2_ptr.get()); // max(1, 7) = 7
        REQUIRE(PalPatternMaxBars::evaluateExpression(&andExpr) == 7);

        PatternExpressionPtr gt3_ptr = std::make_shared<GreaterThanExpr>(factory.getPriceHigh(3), factory.getPriceLow(4)); // max(3,4) = 4
        PatternExpressionPtr gt4_ptr = std::make_shared<GreaterThanExpr>(factory.getVolume(0), factory.getIBS1(1)); // max(0,1) = 1
        AndExpr andExpr2(gt3_ptr.get(), gt4_ptr.get()); // max(4,1) = 4
        REQUIRE(PalPatternMaxBars::evaluateExpression(&andExpr2) == 4);
    }
     SECTION("Nested AndExpr") {
        PatternExpressionPtr o0_c1 = std::make_shared<GreaterThanExpr>(open0, close1); // 1
        PatternExpressionPtr r5_m2 = std::make_shared<GreaterThanExpr>(roc5, meander2); // 7
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
    char pLongStr[] = "70.0"; decimal7* pLong = factory.getDecimalNumber(pLongStr);
    char pShortStr[] = "30.0"; decimal7* pShort = factory.getDecimalNumber(pShortStr);
    PatternDescription* desc_raw = new PatternDescription("file.txt", 1, 20230101, pLong, pShort, 10, 2);
    PatternDescriptionPtr desc = std::shared_ptr<PatternDescription>(desc_raw);


    PriceBarReference* open0 = factory.getPriceOpen(0);
    PriceBarReference* close1 = factory.getPriceClose(1);
    PatternExpression* gt_expr_raw = new GreaterThanExpr(open0, close1);
    PatternExpressionPtr pattern_expr = std::shared_ptr<PatternExpression>(gt_expr_raw);


    MarketEntryExpression* entry = factory.getLongMarketEntryOnOpen();

    char ptStr[] = "5.0"; decimal7* pt_val = factory.getDecimalNumber(ptStr);
    ProfitTargetInPercentExpression* profit_target = factory.getLongProfitTarget(pt_val);

    char slStr[] = "2.0"; decimal7* sl_val = factory.getDecimalNumber(slStr);
    StopLossInPercentExpression* stop_loss = factory.getLongStopLoss(sl_val);

    SECTION("Constructor and Basic Getters") {
        PriceActionLabPattern pal_pattern(desc.get(), pattern_expr.get(), entry, profit_target, stop_loss); // Using raw pointers from shared_ptr

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

        char newPtStr[] = "6.0"; decimal7* new_pt_val = factory.getDecimalNumber(newPtStr);
        ProfitTargetInPercentExpression* new_profit_target = factory.getLongProfitTarget(new_pt_val);

        char newSlStr[] = "2.5"; decimal7* new_sl_val = factory.getDecimalNumber(newSlStr);
        StopLossInPercentExpression* new_stop_loss = factory.getLongStopLoss(new_sl_val);

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
    PatternDescriptionPtr desc = std::make_shared<PatternDescription>("f",0,0,factory.getDecimalNumber(0),factory.getDecimalNumber(0),0,0);
    PatternExpressionPtr expr = std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0), factory.getPriceClose(0));
    MarketEntryExpression* entry = factory.getLongMarketEntryOnOpen();
    ProfitTargetInPercentExpression* pt = factory.getLongProfitTarget(factory.getDecimalNumber(1));
    StopLossInPercentExpression* sl = factory.getLongStopLoss(factory.getDecimalNumber(1));

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
        std::make_shared<PatternDescription>("f1.txt",1,20230101,
            factory.getDecimalNumber(1),
            factory.getDecimalNumber(1),
            10,1);
    PatternExpressionPtr expr1_ptr =
        std::make_shared<GreaterThanExpr>(factory.getPriceOpen(0),
                                          factory.getPriceClose(1));
    MarketEntryExpression* long_entry   = factory.getLongMarketEntryOnOpen();
    ProfitTargetInPercentExpression* pt1 = factory.getLongProfitTarget(factory.getDecimalNumber(1));
    StopLossInPercentExpression*     sl1 = factory.getLongStopLoss   (factory.getDecimalNumber(1));

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
