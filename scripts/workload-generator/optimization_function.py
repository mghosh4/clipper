
# coding: utf-8

# In[106]:


import numpy as np
import numpy.linalg as la
from scipy.optimize import minimize


# In[107]:


# B - array of batch sizes corresponding to each model
# N - array of replication factors corresponding to each model
# Q - array of query rates to each model
# T - array of effective time per query corresponding to each model

# Throughput base definition
def throughput(B, N, Q, T):
    return np.multiply(Q, T) / (1 + np.abs(np.multiply(B, N)))

# Latency base definition
def latency(B, T, lbf):
    return np.multiply(lbf, np.multiply(B, T))

# Note scipy.minimize() requires 1D vector.
# Data shaping utility functions
def flatten(B, N):
    return np.concatenate([B, N])

def reform(X):
    return X.reshape((2, len(X)//2))

def first(X):
    return X[:len(X)//2]

def second(X):
    return X[len(X)//2:]

def display(X):
    print("B: " + str(first(X)))
    print("N: " + str(second(X)))

class OptimizationFunction:
    
    def __init__(self, n, pf, l_max):
        # Number of nodes
        self.n = n
        # Placement factor - number of containers per node
        self.placement_factor = pf
        # Number of placement locations
        self.N_limit = n * pf
        # Max latency - SLO
        self.max_latency = l_max
    
    def optimize(self, B_start, N_start, Q, T, lbf):
        x0 = flatten(B_start, N_start)
        bnds = tuple([(1, None)] * len(x0))
        fun = lambda x : la.norm(throughput(first(x),second(x), Q, T), 1)
        cons = self.constraints(T, lbf)
        return minimize(fun, x0, method='SLSQP', bounds=bnds, constraints=cons, tol=1e-6)
        
    def sum_constraint(self, N):
        return np.sum(N) - self.N_limit
        
    def latency_constraint(self, B, T, lbf):
        return self.max_latency - np.max(latency(B, T, lbf))
        
    def constraints(self, T, lbf):
        cons = (
            {'type': 'eq', 'fun': lambda x: self.sum_constraint(second(x))},
            {'type': 'ineq', 'fun': lambda x: self.latency_constraint(first(x), T, lbf)}
        )
        return cons


# In[108]:


opt_func = OptimizationFunction(6, 4, 20000)
# Use Clipper Figure 3: Model Container Latency Profiles as reference
# a) Linear SVM (SKLearn)
# b) Random Forest (SKLearn)
# c) Kernel SVM (SKLearn)
# d) No-Op
# e) Logistic Regression (SKLearn)
# f) Linear SVM (PySpark)

# Starting batch values - use median batch size from charts
b_start = np.array([350, 200, 1, 600, 700, 300])
# Starting replication values
n_start = 4*np.ones(6)
# Current query rate values
Q = np.array([10, 4, 0, 5, 15, 8])
# Current query time values
T = np.array([14.28,50,10000, 10, 12, 30])
# Latency batch factor const
lbf = np.ones(6)
results = opt_func.optimize(b_start, n_start, Q, T, lbf)


# In[109]:


print("Raw Results")
display(results.x)


# In[110]:


def revert_order(arr, sort_indices):
    mapping = {ind : i for i,ind in enumerate(sort_indices)}
    return [arr[mapping[i]] for i in range(len(arr))]

def normalize_results(x, max_placements):
    reshaped = reform(x)
    B = reshaped[0]
    N = reshaped[1]
    normalized_B = np.round(B)
    normalized_N = normalize_constrained_array(N, max_placements)
    return list(zip(map(int, normalized_B), map(int, normalized_N)))
    
def normalize_constrained_array(s_arr, max_placements):
    int_arr = np.fix(s_arr)
    int_sum = np.sum(int_arr)
    frac_weight_sortinds = np.argsort(-((s_arr - int_arr) / int_arr))
    sorted_int_arr = int_arr[frac_weight_sortinds]
    rem_buf = int(round(np.sum(s_arr) - int_sum))
    i = 0
    while rem_buf > 0:
        sorted_int_arr[i] += 1
        rem_buf -= 1
        i += 1
    return map(int, revert_order(sorted_int_arr, frac_weight_sortinds))


# In[105]:


print("Normalized Results ((B,N) value pairs by model)")
print(", ".join(["%d:%s" % (i,str(x)) for i, x in enumerate(normalize_results(results.x, opt_func.N_limit))]))

