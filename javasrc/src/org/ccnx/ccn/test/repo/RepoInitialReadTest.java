/**
 * A CCNx library test.
 *
 * Copyright (C) 2008, 2009 Palo Alto Research Center, Inc.
 *
 * This work is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation. 
 * This work is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

package org.ccnx.ccn.test.repo;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

import org.ccnx.ccn.impl.repo.RepositoryException;
import org.ccnx.ccn.io.CCNReader;
import org.ccnx.ccn.protocol.ContentName;
import org.ccnx.ccn.protocol.ContentObject;
import org.ccnx.ccn.protocol.Interest;
import org.ccnx.ccn.protocol.PublisherID;
import org.ccnx.ccn.protocol.PublisherPublicKeyDigest;
import org.junit.Assert;
import org.junit.Test;

public class RepoInitialReadTest extends RepoTestBase {
	
	@Test
	public void testReadViaRepo() throws Throwable {
		System.out.println("Testing reading objects from repo");
		ContentName name = ContentName.fromNative("/repoTest/data1");
		
		// Since we have 2 pieces of data with the name /repoTest/data1 we need to compute both
		// digests to make sure we get the right data.
		ContentName name1 = new ContentName(name, ContentObject.contentDigest("Here's my data!"));
		ContentName digestName = new ContentName(name, ContentObject.contentDigest("Testing2"));
		String tooLongName = "0123456789";
		for (int i = 0; i < 30; i++)
			tooLongName += "0123456789";
		
		// Have 2 pieces of data with the same name here too.
		ContentName longName = ContentName.fromNative("/repoTest/" + tooLongName);
		longName = new ContentName(longName, ContentObject.contentDigest("Long name!"));
		ContentName badCharName = ContentName.fromNative("/repoTest/" + "*x?y<z>u");
		ContentName badCharLongName = ContentName.fromNative("/repoTest/" + tooLongName + "*x?y<z>u");
			
		checkDataWithDigest(name1, "Here's my data!");
		checkDataWithDigest(digestName, "Testing2");
		checkDataWithDigest(longName, "Long name!");
		checkData(badCharName, "Funny characters!");
		checkData(badCharLongName, "Long and funny");
		
		CCNReader reader = new CCNReader(getLibrary);
		ArrayList<ContentObject>keys = reader.enumerate(new Interest(keyprefix), 4000);
		for (ContentObject keyObject : keys) {
			checkDataAndPublisher(name, "Testing2", new PublisherPublicKeyDigest(keyObject.content()));
		}
	}
	
	private void checkData(ContentName name, String data) throws IOException, InterruptedException{
		checkData(new Interest(name), data.getBytes());
	}
	
	private void checkDataWithDigest(ContentName name, String data) throws RepositoryException, IOException, InterruptedException {
		// When generating an Interest for the exact name with content digest, need to set maxSuffixComponents
		// to 0, signifying that name ends with explicit digest
		Interest interest = new Interest(name);
		interest.maxSuffixComponents(0);
		checkData(interest, data.getBytes());
	}
	
	private void checkData(Interest interest, byte[] data) throws IOException, InterruptedException{
		ContentObject testContent = getLibrary.get(interest, 10000);
		Assert.assertFalse(testContent == null);
		Assert.assertTrue(Arrays.equals(data, testContent.content()));		
	}

	private void checkDataAndPublisher(ContentName name, String data, PublisherPublicKeyDigest publisher) 
				throws IOException, InterruptedException {
		Interest interest = new Interest(name, new PublisherID(publisher));
		ContentObject testContent = getLibrary.get(interest, 10000);
		Assert.assertFalse(testContent == null);
		Assert.assertEquals(data, new String(testContent.content()));
		Assert.assertTrue(testContent.signedInfo().getPublisherKeyID().equals(publisher));
	}

}