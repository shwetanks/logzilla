import re
import sys

def tokenize(text):
    return re.findall('[a-z0-9]+', text)

def tokenize_independence(string, bigrams):
    unigrams = tokenize(string)
    bigrams  = filter(lambda x:x in bigrams, izip(unigrams, islice(unigrams, 1, None)))
    tokens = unigrams + bigrams
    return tokens

def get_bigrams(data_file):
    unigram_freq = Counter()
    bigram_freq = Counter()

    with open(data_file) as f:
        for line in f:
            parts = line.split('\t')
            words = tokenize(parts[2] + " " + parts[3])
            for w in words:
                unigram_freq[w] += 1.

            for b in izip(words, islice(words, 1, None)):
                bigram_freq[b] += 1.

    unigram_count = sum(unigram_freq.values())
    bigram_count  = sum(bigram_freq.vlues())

    bigrms = []
    for b in bigram_freq.keys():
        if bigram_freq[b] > 100:
            if (bigram_freq[b]/bigram_count) / ((unigram_freq[b[0]] * unigram_freq[b[1]]) / (unigram_count * unigram_count)) > 100:
                bigrams.append(b)

    print bigrams
    return bigrams
