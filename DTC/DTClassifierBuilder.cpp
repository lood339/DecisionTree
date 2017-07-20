//
//  DTClassifierBuilder.cpp
//  Classifer_RF
//
//  Created by jimmy on 2016-09-20.
//  Copyright (c) 2016 Nowhere Planet. All rights reserved.
//

#include "DTClassifierBuilder.h"
#include "dt_random.hpp"
#include <iostream>
#include "dt_util.hpp"
#include "dt_proximity.hpp"
#include "mat_io.hpp"

using std::cout;
using std::endl;

void DTClassifierBuilder::setTreeParameter(const DTCTreeParameter & param)
{
    tree_param_ = param;
}

bool DTClassifierBuilder::buildModel(DTClassifer & model,
                                     const vector<VectorXd> & features,
                                     const vector<unsigned int> & labels,
                                     const vector<VectorXd> & valid_features,
                                     const vector<unsigned int>& valid_labels,
                                     const char * model_file_name) const
{
    assert(features.size() == labels.size());
    assert(valid_features.size() == valid_labels.size());
    
    model.tree_param_ = tree_param_;
    model.trees_.clear();
    
    const int tree_num = tree_param_.tree_num_;
    const int category_num = tree_param_.category_num_;
    const bool is_proximity = tree_param_.proximity_;
    
    const int N = (int)features.size();
    DTProximity data_proximity(N);
    
    for (int n = 0; n<tree_num; n++) {
        // bagging
        vector<unsigned int> training_indices;
        vector<unsigned int> validation_indices;
        DTRandom::outofBagSampling<unsigned int>((unsigned int) features.size(), training_indices, validation_indices);
        
        DTCTree * tree = new DTCTree();
        assert(tree);
        double tt = clock();
        tree->buildTree(features, labels, training_indices, tree_param_);
        model.trees_.push_back(tree);
        
        printf("build tree %d cost %lf minutes\n", n, (clock()- tt)/CLOCKS_PER_SEC/60.0);
        
        // test on the validation data
        vector<unsigned int> cv_predictions;
        vector<unsigned int> cv_labels;
        for (int i = 0; i<validation_indices.size(); i++) {
            const int index = validation_indices[i];
            unsigned int pred;
            model.predict(features[index], pred);
            cv_predictions.push_back(pred);
            cv_labels.push_back(labels[index]);            
        }
        
        Eigen::MatrixXd oob_conf = DTUtil::confusionMatrix(cv_predictions, cv_labels, category_num, false);
        cout<<"Out of bag validation confusion matrix: \n"<<oob_conf<<endl;
        if (model_file_name != NULL) {
            model.save(model_file_name);
        }
        
        if (valid_features.size() != 0) {
            vector<unsigned int> valid_predictions;
            for (int i = 0; i<valid_features.size(); i++) {
                unsigned int pred = 0;
                model.predict(valid_features[i], pred);
                valid_predictions.push_back(pred);
            }
            assert(valid_predictions.size() == valid_labels.size());
            Eigen::MatrixXd valid_conf = DTCUtil::confusionMatrix(valid_predictions, valid_labels, category_num, false);
            Eigen::VectorXd accuracy = DTUtil::accuracyFromConfusionMatrix(valid_conf);
            cout<<"Validation confusion matrix: \n"<<valid_conf<<endl;
            cout<<"Validation accuracy: \n"<<accuracy.transpose()<<endl;
        }
        if (is_proximity) {
            vector<unsigned int> indices = DTUtil::range<unsigned int>(0, (int)features.size(), 1);
            tree->computeProximity(features, indices, data_proximity);
            
            
            // void computeProximityMatrix(const int k);
            data_proximity.computeProximityMatrix(5);            
            const Eigen::MatrixXf prox_mat = data_proximity.getMatrix();
            matio::writeMatrix((string("tree_") + std::to_string(n) + string("_prox.mat")).c_str(),
                               "proximity", prox_mat);
        }
        
        
    }
    return true;
}


bool DTClassifierBuilder::buildModel(DTClassifer & model,
                                     const vector< vector<VectorXd> > & features,
                                     const vector< vector<unsigned int> > & labels,
                                     const int max_num_frames,
                                     const char * model_file_name) const
{
    assert(features.size() == labels.size());
    
    model.tree_param_ = tree_param_;
    model.trees_.clear();   // @todo release memory
    
    
    const int tree_num = tree_param_.tree_num_;
    const int category_num = tree_param_.category_num_;
    for (int n = 0; n<tree_num; n++) {
        // randomly select frames
        vector<Eigen::VectorXd> train_features;
        vector<unsigned int> train_labels;
        for (int i = 0; i<max_num_frames; i++) {
            int rnd_idx = rand()%features.size();
            train_features.insert(train_features.end(), features[rnd_idx].begin(), features[rnd_idx].end());
            train_labels.insert(train_labels.end(), labels[rnd_idx].begin(), labels[rnd_idx].end());
        }
        
        vector<unsigned int> training_indices;
        for (int i = 0; i<train_features.size(); i++) {
            training_indices.push_back(i);
        }
        
        // training
        DTCTree * tree = new DTCTree();
        assert(tree);
        double tt = clock();
        tree->buildTree(train_features, train_labels, training_indices, tree_param_);
        model.trees_.push_back(tree);
        printf("build tree %d cost %lf minutes\n", n, (clock()- tt)/CLOCKS_PER_SEC/60.0);
        
        if (model_file_name != NULL) {
            model.save(model_file_name);
        }
        
        // single tree validataion error
        vector<Eigen::VectorXd> validation_features;
        vector<unsigned int> validation_labels;
        for (int i = 0; i<10; i++) {
            int rnd_idx = rand()%features.size();
            validation_features.insert(validation_features.end(), features[rnd_idx].begin(), features[rnd_idx].end());
            validation_labels.insert(validation_labels.end(), labels[rnd_idx].begin(), labels[rnd_idx].end());
        }
        
        vector<unsigned int> cv_predictions;
        for (int i = 0; i<validation_features.size(); i++) {
            unsigned int pred;
            bool is_pred = model.predict(validation_features[i], pred);
            assert(is_pred);
            if (is_pred) {
                cv_predictions.push_back(pred);
            }
        }
        
        Eigen::MatrixXd confusion = DTCUtil::confusionMatrix(cv_predictions, validation_labels, category_num, true);
        cout<<"cross validation confusion matrix: \n"<<confusion<<" from 10 images.\n";
    }
    
    printf("build model done %lu trees.\n", model.trees_.size());
    return true;
}









