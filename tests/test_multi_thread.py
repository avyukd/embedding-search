import embedding_search as es
import numpy as np
import shutil

def test_main():
	fp = "test/"
	store = es.EmbeddingStore(fp, 2, 1024, 1024)
	for i in range(5):
		for j in range(5):
			store.addEmbedding(np.array([float(i), float(j)]), str(i) + str(j))
	
	closest = store.getKClosest(np.array([0.5, 0.5]), 5, 3, es.EmbeddingStore.DistanceMetric.cosine_similarity) 
	exp_values = ["00", "11", "22", "33", "44"]
	assert(closest == exp_values)
	shutil.rmtree(fp)

if __name__ == "__main__":
	test_main()

