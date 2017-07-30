//
// Created by hclimente on 21/07/2017.
//

#include "gin/shake.h"

Shake::Shake() {
	__gwas = new GWASData();
	__cvgrid = NULL;
}

Shake::~Shake() {
	delete __gwas;
	delete __cvgrid;
}

void Shake::readGWAS(string const& pedBasename, uint encoding) {

	CPlinkParser::readPEDFile(pedBasename + ".ped", __gwas);
	CPlinkParser::readMAPFile(pedBasename + ".map", __gwas);

	CGWASDataHelper::encodeHeterozygousData(__gwas, encoding);
}

void Shake::readNetwork(string const& networkFileame) {

	CSconesIO::readSparseNetworkFile(networkFileame, __gwas);

}

void Shake::searchHyperparameters(uint folds, uint const& scoring_function, uint const& association_score) {

	MatrixXd X = __gwas -> X;
	VectorXd y = __gwas -> Y.col(0);
	SparseMatrixXd W = __gwas -> network;

	UnivariateAssociation univar(__gwas -> X, __gwas -> Y.col(0));

	if (association_score == CHI2) {
		__c = univar.computeChi2();
	} else if (association_score == TREND) {
		// TODO implement different trend scores
		__c = univar.computeTrendTest("additive");
	} else if (association_score == SKAT) {
		__c = univar.computeSKAT();
	}

	__cvgrid = new GridCV(&X, &y, &W, __c, folds, association_score);
	__cvgrid -> exploreGrids(scoring_function);

	__bestEta = __cvgrid -> bestParameters().first;
	__bestLambda = __cvgrid -> bestParameters().second;

}

void Shake::selectSnps() {

	Scones s = Scones(__c, __bestEta, __bestLambda, __gwas -> network);
	__selectedSnps = s.selected();

}

void Shake::writeResults(string const& output) {

	CSconesIO::writeOutput(output, __gwas, __selectedSnps, __bestEta, __bestLambda, __c);

}