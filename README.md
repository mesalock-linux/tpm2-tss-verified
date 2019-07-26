# tpm2-tss-verified

This repository hosts a modified version of [tpm2-tss
2.1.0](https://github.com/tpm2-software/tpm2-tss/tree/35c121f6814beef3dbc369e60a2ca189eb381911) 
that can pass memory safety formal verification.  We utilized [Abstract
Interpretation](https://en.wikipedia.org/wiki/Abstract_interpretation) and the
tool [TrustInSoft Analyzer](https://trust-in-soft.com) to achieve this goal. 
The original TSS readme is renamed to [Original_README.md](Original_README.md).


It took us 9 person-months to verify the overall 1300+ funcitons (or 67K+ lines
of code). During the verification, We found several potential memory safety
flaws (e.g. null pointer dereference, memory leakage, buffer over-read, integer
overflow etc.), and made 140+ patches to address them. Under our constraints,
the program has been proven to be memory safe. Hopefully this effort can help
developers to build memory safe TPM applications, and further contributions
from you are always welcome!

## Getting Started

Our formal verification process makes no changes to the original building or
testing procedures, so please check the [original
instructions](https://github.com/tpm2-software/tpm2-tss/blob/2.1.0/INSTALL.md)
for detailed information.

## Running the tests

Please check the [original instructions](https://github.com/tpm2-software/tpm2-tss/tree/2.1.0#testing) for detailed information.


## Formal Verification Information

The verification result is summarized in
[tpm2-tss-verify-results.md](tpm2-tss-verify-results.md). Each function in this
file is followed by its verification coverage, false/true positive counts,
whether it’s passed or not, and whether it’s patched by us in order to pass the
verification. 

The following are some examples. Please check
[tpm2-tss-verify-results.md](tpm2-tss-verify-results.md) for the full result. 

|Function Name|Coverage	|# of False Positive|# of True Positive|Patched or Not|
|---|:---:|:---:|:---:|:---:|
|Tss2\_MU\_TPMS\_ALG\_PROPERTY\_Marshal | 100% | 1 | 0 | No |
|Tss2\_MU\_TPMS\_ALG\_PROPERTY\_Unmarshal	| 100% | 1 | 0 | No |
|Tss2\_MU\_TPMS\_CAPABILITY\_DATA\_Marshal | 100% | 0 | 0 | No |
|Tss2\_MU\_TPMS\_CAPABILITY\_DATA\_Unmarshal | 100% | 0 | 0 | No |
|Tss2\_MU\_TPMS\_TIME\_INFO\_Unmarshal | 100% | 0 | 0 |  No |

## Issues Found

* [Potential memory leaking in iesys_nv_get_name](https://github.com/tpm2-software/tpm2-tss/issues/1361)
* [Uninitialized variable is referenced in Esys_Quote](https://github.com/tpm2-software/tpm2-tss/issues/1362)
* [memory leak in iesys_cryptossl_pk_encrypt](https://github.com/tpm2-software/tpm2-tss/issues/1380)
* [Incorrect SAFE_FREE(name) in Esys_TR_GetName](https://github.com/tpm2-software/tpm2-tss/issues/1432)
* [Incorrect stop condition for the for loop in iesys_compute_hmac](https://github.com/tpm2-software/tpm2-tss/issues/1440)
* [Incorrect null check for esys_context in Esys_TR_FromTPMPublic_Finish](https://github.com/tpm2-software/tpm2-tss/issues/1458)

## Contributing
Contrbutions are welcome. This modified version is based on tpm2-tss 2.1.0. We are happy if you're interested in verifying the functions newly added in newer releases. Feel free to send us pull requests on the GitHub and join the verification journey with us. If you have questions, you can also talk to our maintainers for help.

## Discussion

Feel free to submit Github issues, pull requests, or contact the following maintainers.

- Minghua Wang: [Github](https://github.com/MinghuaWang), [Email](mailto:wangminghua01@baidu.com)
- Yulong Zhang: [Github](https://github.com/yulongzhang), [Email](mailto:ylzhang@baidu.com)
