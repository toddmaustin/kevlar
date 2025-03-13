# Kevlar: Durable Sequestered Encryption Defenses for Memory Vulnerabilities

*Cryptography is the only superpower in computer security.*

This is the mantra of Kevlar and the approach it uses to protect sensitive C++ variables from memory vulnerabilities. Kevlar is a C++ extension that provides programmer with encrypted data types that work exactly like built-in data types, but they are protected from memory attacks with strong encryption. To protect these variables, Kevlar wraps sensitive data with randomized, authenticated, and sequestered encryption. Once wrapped with  encryption, sensitive C++ variables are protected against memory attacks ***despite*** the vulnerabilities in the memory system, caches, store buffers, DRAM, etc.

Let's dive into the defensive features of Kevlar:

* **Randomized encryption** - When Kevlar encrypts a data value, it first packs it with true random salt, such that the resulting ciphertext never correlates with the value being encrypted. For example, assign "42" to an encrypted variable 1M times and you will produce 1M different ciphertexts. This feature prevents cryptanalysis attacks on program ciphertext (.e.g, CIPHERLEAKs), in the event it is disclosed or leaked by memory system vulnerabilities.
* **Authenticated encryption** - When Kevlar decrypts a data value, it can tell if the value decrypted was valid ciphertext. As such, it is not possible for an attacker to forge ciphertext values without being detected. In addition, attacks that perturb memory system integrity such as Rowhammer attacks that can flip bits in DRAM, will be detected when the encrypted variable is next used in the program. Moreover, Kevlar will attempt to correct bit-errors in the invalid ciphertext, and if it is able to correct the error, it will continue the program with the correct ciphertext.
* **Sequestered encryption** - Kevlar performs its cryptography with an AES-128 implementation with its keys and plaintext sequestered to the CPUs register file (i.e., XMM vector registers). When a program performs an operation on a Kevlar encrypted variable, the ciphertext is loaded into a register, decrypted with an in-register key (and keytable), computed upon in plaintext form, and then encrypted and returned to memory as ciphertext. Using sequestered encryption, plaintext and key data only exists in registers. Thus, if the memory attack cannot read the register (e.g., cold-boot, Spectre, Rowhammer), it cannot see anything about the Kevlar-protected data except its ciphertext.
* **Leaky program behavior detection** - While Kevlar uses strong encryption to prevent direct disclosures of protected variable, program behaviors, such as branch behavior and memory access, can indirectly reveal information about sensitive .  These side channels, as they are called, must be eliminated by eliminiting dangerous leaky program behaviors. To assist programmers in writing more secure software, Kevlar-protected variable support (a limited form of )self-reflection, a mechanism that allows them to detect when they are used in a dangerous manner. When leaky behaviors are detected, a Kevlar-protected variable will issue to one-time warning, which will allow the program to debug and excise the dangerous behavior.

The current release of Kevlar is a technology demonstration, which shows the capabilities of sequestered encryption with implementation limitation details in this README.  Kevlar requires an x86 processor with SSE vector extensions and use of the GCC compiler.

## Examples of Attacks Stopped by Kevlar

Kevlar protects variables against a wide range of memory vulnerabilities, in particular those that attempt to disclose memory variables or impact memory integrity. Kevlar is a ***highly durable defense against***:

* **Spectre/Meltdown** - These attacks leak memory values through mispeculation priming and microarchitecture side channels. Kevlar is a strong durable defense against all know variants (e.g., V1, V2, Foreshadow, Fallout, ZombieLoad, etc.). Any existing attack will only leak pure random ciphertext.
* **Rowhammer** - Kevlar will detect all known variants of Rowhammer attacks, through the use of authenticated encryption. After any Rowhammer attack, the next read of a Kevlar-protected variable will signal a decryption authentication error. When a decryption authentication fails, Kevlar will attempt to repair the corrupted ciphertext. If the number of bit-corruptions is small, Kevlar will be able to correct the ciphertext and complete the operation; otherwise, the Rowhammer attack is detected and the program is terminated. If Rowhammer reads are attempted (e.g., RAMBleed), the attacker will only see ciphertext.
* **Cold-boot Attacks** - If the DRAM is super-cooled and transferred to a less secure system, Kevlar defenses will ensure that no sensitive variables in memory can be decrypted. Because Kevlar uses sequestered encryption, the key information needed to decrypt stolen memory remains in the original system.
* **Buffer Over-reads** - When a buffer over-read occurs (e.g., HeartBleed), Kevlar will ensure that only ciphertext is revealed to the attacker, since all Kevlar-protected data in memory is always encrypted. Moreover, the use of sequestered encryption ensures that if the attacker continues to scan memory, they will no find the keys necessary to decrypt any protected ciphertext (since it remain always in CPU registers).
* **Ciphertext analysis attacks** - If an attacker attempts to reverse-engineer ciphertext in memory (e.g. , CIPHERLEAKS), the attacker will not find any information contained in the produced ciphertect. Kevlar injects random entropy into every ciphertext it creates, ensuring that any ciphertext it produces appears as a true random value. In addition, each ciphertext contains an authentication code, and thus, chosen-plaintext attacks (CPA) will not be possible without Kevlar detecting the attack.
* **Software and hardware side channels** - If an attacker attempts to infer information about a system secret by observing publicly visible aspects of the system (e.g., PRIME+PROBE, PortSmash), the leaky program behavior detectors that Kevlar-protected variables provide will assist programmer in excising the leaky behaviors that their programs possess. Once this process is complete, the program will no longer exhibit behaviors that belie its underlying secrets.

## Running the Kevlar Technology Demonstration

The Kevlar technology demonstration includes an implementation of the "enc_uint64_t" encrypted 64-bit unsigned integer type for C++. Any program variable that declares its variable with this type will receive Kevlar protections. To run Kevlar, you will have to meet some specific system requirements:

* GNU G++ on Linux, with G++ at least version 11.4 (earlier versions may work but they have not yet been tested).
* An x86 CPU with support for the SSE2 vector instruction set extension. This should include any Intel or AM processor sold in the last decade.

To run the technology demo, check out the Kevlar git repo from: https://github.com/toddmaustin/kevlar, and then run the following command line:

```
make clean build test
```

This command will first "clean" the Kevlar directory and then "build" the demonstration application, and "test" it by running a battery of tests. If the demonstration application functions correctly, the output will look as follows:

```
Testing type: uint64_t
Testing 'x'...
WARNING: Program behaviors are likely leaking secrets!
'x' is non-zero...
     a: 7f5d23d0380bccfdb671cab1901097fd
aprime: 47e0dbc8be1a4d46452bf7520615f4d1
     b: d7ed9073239b8e9e710482e4bd5aa0c6
     c: 1e071d460d6b0168b26520a5dc005952
     d: e5146cdd7cbbcd178030892fbebabf97
     e: f1acb6f65cc6133a2b4070c55b0318e5
     f: 0a68ee694200dba1ceca7c80d33be93a
     g: 9bb79db5cbbbf6b7894651c209ffcfad
     h: c56e1c6a798ffc24614a60e901aeaee7
     i: d4a4eb2fe0ae84e60bb94190b35ea882
HACK: Flipping bit 8 of protected variable `j'
ERROR: Decryption authentication failure!
NOTE: Attempting recovery of corrupted ciphertext...
NOTE: Ciphertext was fixed! (Flipped bit `8')
     j: 0d9b2be88b36b90b59612ee4bab96c37
All tests passed for uint64_t.
INFO: Resetting leaky behavior detectors.
INFO: Running data-heuristic ISQRT() algorithm...
WARNING: Program behaviors are likely leaking secrets!
INFO: The integer square root of '975461057789971041' is '987654321',
INFO: Resetting leaky behavior detectors.
INFO: Running data-oblivious ISQRT() algorithm...
INFO: The integer square root of '975461057789971041' is '987654321',
```

The technology demonstration application does the following: 1) run a sequence of encrypted computation, checking the results of the computation. 2) Flips a bit in the ciphertext of one of the later computation, and then corrects the bit-flip. 3) runs an ISQRT function that implement Newton's Algorithm, first with data heuristics that set off the leaky detectors, and again, with a data-oblivious implementation that does not set of the leakage detectors.

If you do not see the output above, then there is probably some aspect of your compiler, operating systems, or CPU that is impeding Kevlar's correct operation. If this happens, please don't hesitate to submit an issue on GitHub, and we will try to fix the problem. Good luck!

## Using Kevlar to Protect Your Own Programs

TBD...

## What are the Current Limitations of Kevlar?

TBD...

## To Learn More about Kevlar and Sequestered Encryption...

Kevlar was built to demonstrate the strong defenses given to software by sequestered encryption. Yet, in its current form, Kevlar is only a small demonstration of the full capabilities of sequestered encryption.

To learn more about sequestered encryption, read this paper: ["Sequestered Encryption: A Hardware Technique for Comprehensive Data Privacy"](https://drive.google.com/file/d/1d7YQEsHqLNkNqSXXbTNtEFzDhbQRt155/view?usp=drive_link)

Or, check out this short presentation: ["Sequestered Encryption Primer"](https://drive.google.com/file/d/160teigtxkxjOqAOvT4ttlj-_WoQmb_BI/view?usp=drive_link)

Or, visit Agita Labs (http://agitalabs.com), which is a startup the has deployed a hardware version of sequestered encryption in the Microsoft Azure and Amazon AWS clouds. Their version of sequestered encryption is much more powerful, providing a full privacy-oriented programming environment that allows cloud developers to compute on third-party data without the ability to see that data. Check out the website for more information.

## Licensing details

The Kevlar technology demonstration code was written by Todd Austin, and it is available for use under the [BSD license](https://en.wikipedia.org/wiki/BSD_licenses).

