## Synopsis and motivation

Language models (LMs) are an essential element in statistical approaches to natural language processing for tasks such as speech recognition and machine translation (MT).
The advent of big data leads to the availability of massive amounts of data to build LMs, and in fact, for the most prominent languages, using current techniques and hardware, it is not feasible to train LMs with all the data available nowadays.
At the same time, it has been shown that the more data is used for a LM the better the performance, e.g. for MT, without any indication yet of reaching a plateau.
This paper presents CloudLM, an open-source cloud-based LM intended for MT, which allows to query distributed LMs.
CloudLM relies on Apache Solr and provides the functionality of state-of-the-art language modelling (it builds upon KenLM), while allowing to query massive LMs (as the use of local memory is drastically reduced), at the expense of slower decoding speed.

## Installation

TODO

Moses Tutorial installation: http://www.statmt.org/moses/?n=Development.GetStarted

## License

CloudLM has the same license than "Moses - factored phrase-based language decoder".