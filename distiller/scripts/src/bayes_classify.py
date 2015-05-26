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
        p = math.log(priors[c])
        n = float(sum(likelihood[c].values()))
        for word in tokenize(line[2]):
            p = p + math.log(max(1E-6, likelihood[c][word] / n))
        if p > max_class[0]:
            max_class = (p, c)
    return max_class[1]


def read_testing_data(filename):
    return [line.strip().split('\t') for line in open(filename).readlines()]

def main():
    training_data = sys.argv[1]
    testing_data = sys.argv[2]

    (priors, likelihood) = read_training_data(training_data)
    lines = read_testing_data(testing_data)

    num_correct = 0
    for line in lines:
        if (classify_max_prior(line, priors, likelihood) == line[1]:
            num_correct += 1
    print "Classified %d correctly out of %d for accuracy:%f" % (num_correct, len(lines), float(num_correct)/len(lines))

if __name__ == '__main__':
    main()
