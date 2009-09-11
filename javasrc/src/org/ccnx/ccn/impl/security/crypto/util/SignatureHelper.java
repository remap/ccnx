/**
 * Part of the CCNx Java Library.
 *
 * Copyright (C) 2008, 2009 Palo Alto Research Center, Inc.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation. 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details. You should have received
 * a copy of the GNU Lesser General Public License along with this library;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301 USA.
 */

package org.ccnx.ccn.impl.security.crypto.util;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.security.AlgorithmParameters;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.security.SignatureException;
import java.security.spec.InvalidParameterSpecException;

import org.bouncycastle.asn1.ASN1InputStream;
import org.bouncycastle.asn1.DEREncodable;
import org.bouncycastle.asn1.DERObjectIdentifier;
import org.bouncycastle.asn1.DERTags;
import org.bouncycastle.asn1.DERUnknownTag;
import org.bouncycastle.asn1.x509.AlgorithmIdentifier;
import org.ccnx.ccn.config.SystemConfiguration;
import org.ccnx.ccn.config.SystemConfiguration.DEBUGGING_FLAGS;
import org.ccnx.ccn.impl.support.Log;


/** 
 * Helper class for generating signatures.
 * @author smetters
 *
 */
public class SignatureHelper {
	
	public static byte [] sign(String digestAlgorithm,
							   byte [] toBeSigned,
							   PrivateKey signingKey) throws SignatureException, 
							   			NoSuchAlgorithmException, InvalidKeyException {
		if (null == toBeSigned) {
			Log.info("sign: null content to be signed!");
			throw new SignatureException("Cannot sign null content!");
		}
		if (null == signingKey) {
			Log.info("sign: Signing key cannot be null.");
			Log.info("Temporarily generating fake signature.");
			return DigestHelper.digest(digestAlgorithm, toBeSigned);
		}
		String sigAlgName =
			getSignatureAlgorithmName(((null == digestAlgorithm) || (digestAlgorithm.length() == 0)) ?
					DigestHelper.DEFAULT_DIGEST_ALGORITHM : digestAlgorithm,
					signingKey);
		// DKS TODO if we switch to SHA256, this fails.
		Signature sig = Signature.getInstance(sigAlgName);

		sig.initSign(signingKey);
		sig.update(toBeSigned);
		return sig.sign();
	}
	
	/**
	 * Sign concatenation of the toBeSigneds.
	 * @param digestAlgorithm
	 * @param toBeSigneds
	 * @param signingKey
	 * @return
	 * @throws SignatureException
	 * @throws NoSuchAlgorithmException
	 * @throws InvalidKeyException
	 */
	public static byte [] sign(String digestAlgorithm,
							   byte [][] toBeSigneds,
							   PrivateKey signingKey) throws SignatureException,
							   	NoSuchAlgorithmException, InvalidKeyException {
		if (null == toBeSigneds) {
			Log.info("sign: null content to be signed!");
			throw new SignatureException("Cannot sign null content!");
		}
		
		if (null == signingKey) {
			Log.info("sign: Signing key cannot be null.");
			Log.info("Temporarily generating fake signature.");
			return DigestHelper.digest(digestAlgorithm, toBeSigneds);
		}
		String sigAlgName =
			getSignatureAlgorithmName(((null == digestAlgorithm) || (digestAlgorithm.length() == 0)) ?
					DigestHelper.DEFAULT_DIGEST_ALGORITHM : digestAlgorithm,
					signingKey);

		Signature sig = Signature.getInstance(sigAlgName);

		sig.initSign(signingKey);
		for (int i=0; i < toBeSigneds.length; ++i) {
			sig.update(toBeSigneds[i]);
		}
		return sig.sign();
	}
	
	public static boolean verify(
			byte [][] data,
			byte [] signature,
			String digestAlgorithm,
			PublicKey verificationKey) throws SignatureException, 
						NoSuchAlgorithmException, InvalidKeyException {
		if (null == verificationKey) {
			Log.info("verify: Verifying key cannot be null.");
			throw new IllegalArgumentException("verify: Verifying key cannot be null.");
		}

		String sigAlgName =
			getSignatureAlgorithmName(((null == digestAlgorithm) || (digestAlgorithm.length() == 0)) ?
					DigestHelper.DEFAULT_DIGEST_ALGORITHM : digestAlgorithm,
					verificationKey);
		
		Signature sig = Signature.getInstance(sigAlgName);

		sig.initVerify(verificationKey);
		if (null != data) {
			for (int i=0; i < data.length; ++i) {
				sig.update(data[i]);
			}
		}
		return sig.verify(signature);
	}
	
	public static boolean verify(byte [] data, byte [] signature, String digestAlgorithm,
										PublicKey verificationKey) 
					throws InvalidKeyException, SignatureException, NoSuchAlgorithmException {
		return verify(new byte[][]{data}, signature, digestAlgorithm, verificationKey);
	}

	
	/**
	 * gets an AlgorithmIdentifier incorporating a given digest and
	 * encryption algorithm, and containing any necessary prarameters for
	 * the signing key
	 *
	 * @param hashAlgorithm the JCA standard name of the digest algorithm
	 * (e.g. "SHA1")
	 * @param signingKey the private key that will be used to compute the
	 * signature
	 *
	 * @throws NoSuchAlgorithmException if the algorithm identifier can't
	 * be formed
	 */
	public static AlgorithmIdentifier getSignatureAlgorithm(
			String hashAlgorithm, PrivateKey signingKey)
	throws NoSuchAlgorithmException, InvalidParameterSpecException, 
	InvalidAlgorithmParameterException
	{
		if (SystemConfiguration.checkDebugFlag(DEBUGGING_FLAGS.DEBUG_SIGNATURES)) {
			Log.warning(
					"SignatureHelper: getSignatureAlgorithm, hash: " +
					hashAlgorithm + " key alg: " + signingKey.getAlgorithm());
		}
		String signatureAlgorithmOID = getSignatureAlgorithmOID(
				hashAlgorithm, signingKey.getAlgorithm());
	
		if (signatureAlgorithmOID == null) {
			if (SystemConfiguration.checkDebugFlag(DEBUGGING_FLAGS.DEBUG_SIGNATURES)) {
				Log.warning("Error: got no signature algorithm!");
			}
			throw new NoSuchAlgorithmException(
					"Cannot determine OID for hash algorithm "+ hashAlgorithm + " and encryption alg " + signingKey.getAlgorithm());
		}
	
		AlgorithmIdentifier thisSignatureAlgorithm = null;
		try {
	
			DEREncodable paramData = null;
			AlgorithmParameters params = OIDLookup.getParametersFromKey(signingKey);
	
			if (params == null) {
				paramData = new DERUnknownTag(DERTags.NULL, new byte [0]);
			} else {
				ByteArrayInputStream bais = new ByteArrayInputStream(params.getEncoded());
				ASN1InputStream dis = new ASN1InputStream(bais);
				paramData = dis.readObject();
			}
	
	
			// Now we need the OID and the parameters. This is not the most
			// efficient way in the world to do this, but it should work.
			thisSignatureAlgorithm =
				new AlgorithmIdentifier(new DERObjectIdentifier(signatureAlgorithmOID),
						paramData);
		} catch (IOException ex) {
			System.out.println("This should not happen: getSignatureAlgorithm -- " );
			System.out.println("    IOException thrown when decoding a key");
			ex.getMessage();
			ex.printStackTrace();
			throw new InvalidParameterSpecException(ex.getMessage());
		} 
		return thisSignatureAlgorithm;
	}

	/**
	 * gets the JCA string name of a signature algorithm, to be used with
	 * a Signature object
	 *
	 * @param hashAlgorithm the JCA standard name of the digest algorithm
	 * (e.g. "SHA1")
	 * @param signingKey the private key that will be used to compute the
	 * signature
	 *
	 * @returns the JCA string alias for the signature algorithm
	 */
	public static String getSignatureAlgorithmName(
			String hashAlgorithm, PrivateKey signingKey)
	{
		return getSignatureAlgorithmName(hashAlgorithm, signingKey.getAlgorithm());
	}

	public static String getSignatureAlgorithmName(
			String hashAlgorithm, PublicKey publicKey)
	{
		return getSignatureAlgorithmName(hashAlgorithm, publicKey.getAlgorithm());
	}

	/**
	 * gets the JCA string name of a signature algorithm, to be used with
	 * a Signature object
	 *
	 * @param hashAlgorithm the JCA standard name of the digest algorithm
	 * (e.g. "SHA1")
	 * @param signingKey the private key that will be used to compute the
	 * signature
	 *
	 * @returns the JCA string alias for the signature algorithm
	 */
	public static String getSignatureAlgorithmName(
			String hashAlgorithm, String keyAlgorithm)
	{
		String signatureAlgorithm = OIDLookup.getSignatureAlgorithm(hashAlgorithm,
				keyAlgorithm);
		//Library.info("getSignatureName: combining " +
		//			hashAlgorithm  + " and " + keyAlgorithm +
		//			" results in: " + signatureAlgorithm);
		return signatureAlgorithm;
	}

	/**
	 * gets the OID of a signature algorithm, to be used with
	 * a Signature object
	 *
	 * @param hashAlgorithm the JCA standard name of the digest algorithm
	 * (e.g. "SHA1")
	 * @param signingKey the private key that will be used to compute the
	 * signature
	 *
	 * @returns the JCA string alias for the signature algorithm
	 */
	public static String getSignatureAlgorithmOID(
			String hashAlgorithm, String keyAlgorithm)
	{
		String signatureAlgorithm = OIDLookup.getSignatureAlgorithmOID(hashAlgorithm,
				keyAlgorithm);
	//	Library.info("getSignatureAlgorithmOID: combining " +
	//				hashAlgorithm  + " and " + keyAlgorithm +
	//				" results in: " + signatureAlgorithm);
		return signatureAlgorithm;
	}
}