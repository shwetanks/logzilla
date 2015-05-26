import math
import random
from collections import Counter, defaultdict
import re
import sys

def tokenize(text):
    return re.findall('[a-z0-9]+', text)

def read_training_data(filename):
    priors = Counter()
    likelihood = defaultdict(Counter)
    
    with open(filename) as f:
        for line in f:
            parts = line.split('\t')
            priors[parts[1]]+=1
            for word in tokenize(parts[2]):
                likelihood[parts[1]][word] += 1

    return (priors, likelihood)

def classify_random(line, priors, likelihood):
    categories = priors.keys()
    return categories[int(random.random() * len(categories))]

def classify_max_prior(line, priors, likelihood):
    return max(priors, key=lambda x: priors[x])

def classify_bayesian(line, priors, likelihood):
    max_class = (-1E6, '')
    for c in priors.keys():
        p = priors[c]
        for word in tokenize(line[2]):
            p = p * max(1E-4, likelihood[c][word])

        if p > max_class[0]:
            max_class = (p, c)

    return max_class[1]


def read_testing_data(filename):
    return [line.strip().split('\t') for line in open(filename).readlines()]


def get_class_posteriors(line, priors, likelihood):
    """E step: return class maximizing posterior"""
    max_class = (-1E6, '')
    posteriors = Counter()

    for c in priors.keys():
        p = priors[c]
        for word in tokenize(line[2]):
            p = p * max(1E-4, likelihood[c][word])

        posteriors[c] = p

    total = sum(posteriors.values())
    if total = 0.0:
        return posteriors

    for c in posteriors.keys():
        posteriors[c] = posteriors[c] / total

    return posteriors


def get_lines_from_files(filename):
    return [line.strip.split('\t') for line in open(filename).readlines()]


def relearn_prior_likelihood(plines):
    """ M step: use E-steps' classification to get priors, likelihood """
    priors = Counter()
    likelihood = defaultdict(Counter)
    
    for(posterior, line) in plines:
        for k in posterior.keys():
            priors[k] += posterior[k]
            for word in tokenize(line[2]):
                likelihood[k][word] += posterior[k]

    return (priors, likelihood)


def get_posteriors_from_lines(lines, keys):
    """ set posterior to be 1.0 for correct category, 0.0 otherwise"""
    labelled_posteriors = []
    for line in lines:
        posteriors = Counter()
        for k in keys:
            posteriors[k] = 0
        posteriors[line[1]] = 1.
        labelled_posteriors.append((posteriors, line))
    return labelled_posteriors


def get_priors_likelihood_from_file(filename):
    """ return priors and likelihood for words in file """
    priors = Counter()
    likelihood = defaultdict(Counter)

    with open(filename) as f:
        for line in f:
            parts = line.split('\t')
            priors[parts[1]] += 1
            for word in tokenize(parts[2]):
                likelihood[parts[1]][word] += 1

    return (priors, likelihood)
    

def main():
    training_file = sys.argv[1]
    testing_file = sys.argv[2]

    (priors, likelihood) = get_priors_likelihood_from_file(training_file)

    testing_lines = get_lines_from_file(testing_file)
    training_lines = get_lines_from_file(training_file)

    labelled_posteriors = get_posteriors_from_lines(training_lines, priors.keys())

    for i in range(10):
        unlabelled_posteriors = [] #contains posteriors and lines
        num_correct = 0

        #normalize likelihood
        for k in priors.keys():
            n = float(sum(likelihood[k].values()))
            for v in likelihood[k].keys():
                likelihood[k][v] /= n

        num_lines = len(testing_lines)
        num_classified = 0
        for line in testing_lines:
            classification = classify_bayesian(line, priors, likelihood)
            num_classified += 1
            if classification == line[1]:
                num_correct += 1
            #else:
            #     print "bad classification", line[1], classification
            unlabelled_posteriors.append((get_class_posteriors(line, priors, likelihood), line))

        print "Classified %d correctly out of %d for an accuracy of %f" %(num_correct, len(testing_lines), float(num_correct)/len(testing_lines))
        #print "priors, likelihood before relearning: ", priors, likelihood
        (priors, likelihood) = relearn_priors_likelihood(labelled_posteriors + unlabelled_posteriors)
        #print "priors, likelihood after relearning: ", priors, likelihood
        
if __name__ == '__main__':
    main()
