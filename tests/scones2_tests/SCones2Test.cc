#include "gtest/gtest.h"
#include "CEasyGWAS/gwas/CScones.h"
#include "CEasyGWAS/io/CSconesIO.h"
#include "CEasyGWAS/io/CPlinkParser.h"
#include "CEasyGWAS/globals.h"

struct CSconesInitialSettings{

    float64 eta;
    float64 expected_eta;
    float64 lambda;
    float64 expected_lambda;
    uint test_statistic;
    float64 expected_association;
    float64 expected_connectivity;
    float64 expected_sparsity;
    string path_prefix;
    int expected_causal_SNPs[10];

};

struct SearchMarkers : public ::testing::Test, testing::WithParamInterface<CSconesInitialSettings> {

    CSconesSettings settings;
    CScones* scones;
    float64 eta;
    float64 lambda;
    GWASData tmpData;

    SearchMarkers(){

        GWASData data;

        string genotype_str = GetParam().path_prefix + "genotype";
        string phenotype_str = GetParam().path_prefix + "phenotype.txt";
        string network_str = GetParam().path_prefix + "network.txt";
        uint encoding = 0;
        float64 maf = 0.05;

        CPlinkParser::readPEDFile(genotype_str + ".ped", &data);
        CPlinkParser::readMAPFile(genotype_str + ".map", &data);
        CPlinkParser::readPhenotypeFile(phenotype_str,&data);
        CGWASDataHelper::encodeHeterozygousData(&data,encoding);
        CGWASDataHelper::filterSNPsByMAF(&data,maf);
        CSconesIO::readSparseNetworkFile(network_str,&data);
        tmpData = CGWASDataHelper::removeSamples4MissingData(data,0);

        eta = GetParam().eta;
        lambda = GetParam().lambda;

        settings = CSconesSettings();
        settings.folds = 10;
        settings.seed = 0;
        settings.selection_criterion = CONSISTENCY;
        settings.selection_ratio = 0.8;
        settings.test_statistic = GetParam().test_statistic;
        settings.nParameters = 10;
        settings.evaluateObjective = false;
        settings.dump_intermediate_results = true;
        settings.dump_path = "tmp/";

        if (eta >= 0 & lambda >= 0) {
            // set up specific lambda and eta
            VectorXd l(1);
            l(0) = lambda;
            VectorXd e(1);
            e(0) = eta;
            settings.lambdas = l;
            settings.etas = e;
            // avoid gridsearch
            settings.autoParameters = false;
        } else {
            settings.lambdas = VectorXd::Zero(settings.nParameters);
            settings.etas = VectorXd::Zero(settings.nParameters);
            settings.autoParameters = true;
        }

        scones = new CScones(tmpData.Y.col(0),tmpData.X,tmpData.network, settings);
    }

    ~SearchMarkers(){
        delete scones;
    }
};

TEST_P(SearchMarkers, checkObjectiveFunctionTerms) {
    auto as = GetParam();
    scones -> test_associations();
    float64 lambda = scones -> getBestLambda();
    float64 eta = scones -> getBestEta();
    VectorXd terms = scones -> getObjectiveFunctionTerms(lambda,eta);

    double association = terms(0);
    double connectivity = terms(1);
    double sparsity = terms(2);

    EXPECT_NEAR(as.expected_association, association, 1);
    EXPECT_NEAR(as.expected_connectivity, connectivity, 1);
    EXPECT_NEAR(as.expected_sparsity, sparsity, 1);

}

TEST_P(SearchMarkers, checkLambdaAndEta) {
    auto as = GetParam();

    scones -> test_associations();
    float64 lambda = scones -> getBestLambda();
    float64 eta = scones -> getBestEta();

    EXPECT_NEAR(as.expected_lambda, lambda , 0.001);
    EXPECT_NEAR(as.expected_eta, eta, 0.001);
}

TEST_P(SearchMarkers, checkSelectedSNPS) {
    auto as = GetParam();

    scones -> test_associations();
    int out = scones -> getIndicatorVector().sum();
    EXPECT_EQ(10, out);
    for (unsigned int i = 0; i < sizeof(as.expected_causal_SNPs)/sizeof(as.expected_causal_SNPs[0]); i++){
        EXPECT_EQ(1, scones -> getIndicatorVector()(as.expected_causal_SNPs[i]));
    }
}

TEST_P(SearchMarkers, checkSelectedSNPs_fixedParameters) {
    auto as = GetParam();

    scones -> test_associations();
    VectorXd indicator_gridsearch = scones -> getIndicatorVector();
    scones -> test_associations(as.expected_lambda, as.expected_eta);
    VectorXd indicator_fixed = scones -> getIndicatorVector();

    EXPECT_EQ(indicator_gridsearch, indicator_fixed);
}

TEST_P(SearchMarkers, checkOutputFiles) {
    auto as = GetParam();

    scones -> test_associations();
    VectorXd terms = scones -> getObjectiveFunctionTerms(scones -> getBestLambda(), scones -> getBestEta());
    VectorXd skat = scones -> getScoreStatistic();

    string output_str = as.path_prefix + "/" + tmpData.phenotype_names[0] + ".scones.out.ext.txt";
    CSconesIO::writeOutput(output_str, tmpData, scones -> getIndicatorVector(), scones -> getBestLambda(), scones -> getBestEta(), terms, skat);

    output_str = as.path_prefix  + "/" + tmpData.phenotype_names[0] + ".scones.pmatrix.txt";
    CSconesIO::writeCMatrix(output_str, scones -> getCMatrix(), scones -> getSettings());
}

CSconesInitialSettings gridSearchParams_skat = CSconesInitialSettings {
        -1, // eta
        16681.00537, // expected_eta
        -1, // lambda
        278.25594, // expected_lambda
        SKAT, // test_statistic
        724379, // expected_association
        44799.20, // expected_connectivity
        166810, // expected_sparsity
        "data/testing/scones/skat/", // path_prefix
        {676, 679, 680, 682, 684, 685, 686, 690, 695, 696} // expected_causal_SNPs
};

CSconesInitialSettings fixedParams_skat = CSconesInitialSettings {
        17000, // eta
        17000, // expected_eta
        300, // lambda
        300, // expected_lambda
        SKAT, // test_statistic
        724379, // expected_association
        48300, // expected_connectivity
        170000, // expected_sparsity
        "data/testing/scones/skat/", // path_prefix
        {676, 679, 680, 682, 684, 685, 686, 690, 695, 696} // expected_causal_SNPs
};

CSconesInitialSettings gridSearchParams_chisq = CSconesInitialSettings {
        41.25, // eta
        16681.00537, // expected_eta
        8.38e-12, // lambda
        278.25594, // expected_lambda
        CHISQ, // test_statistic
        724379, // expected_association
        44799.20, // expected_connectivity
        166810, // expected_sparsity
        "data/testing/scones/skat/", // path_prefix
        {676, 679, 680, 682, 684, 685, 686, 690, 695, 696} // expected_causal_SNPs

};

INSTANTIATE_TEST_CASE_P(checkParameters, SearchMarkers,
    testing::Values(
            gridSearchParams_skat,
            fixedParams_skat,
            gridSearchParams_chisq
    ));


int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}