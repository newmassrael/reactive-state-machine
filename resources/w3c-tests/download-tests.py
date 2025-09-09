#!/usr/bin/env python3
"""
W3C SCXML Test Suite Downloader
Downloads and processes W3C SCXML 1.0 test cases for compliance testing.
"""

import os
import urllib.request
import xml.etree.ElementTree as ET
import argparse
import sys
import re


class W3CTestDownloader:
    def __init__(self, output_dir="w3c-tests"):
        self.output_dir = output_dir
        self.base_url = "https://www.w3.org/Voice/2013/scxml-irp"
        self.manifest_url = f"{self.base_url}/manifest.xml"
        
    def download_file(self, url, path):
        """Download a file from URL to local path."""
        try:
            urllib.request.urlretrieve(url, path)
            print(f"Downloaded: {path}")
            return True
        except Exception as e:
            print(f"Error downloading {url}: {e}")
            return False
    
    def parse_manifest(self, manifest_path):
        """Parse the W3C test manifest to extract test information."""
        tree = ET.parse(manifest_path)
        root = tree.getroot()
        
        tests = []
        for assert_elem in root.findall('assert'):
            assert_id = assert_elem.get('id')
            specnum = assert_elem.get('specnum')
            conformance = assert_elem.find('test').get('conformance')
            manual = assert_elem.find('test').get('manual') == 'true'
            
            # Extract test description
            cdata = assert_elem.text.strip() if assert_elem.text else ""
            
            # Get test URI
            start_elem = assert_elem.find('.//start')
            if start_elem is not None:
                test_uri = start_elem.get('uri')
                
                tests.append({
                    'id': assert_id,
                    'specnum': specnum,
                    'conformance': conformance,
                    'manual': manual,
                    'description': cdata,
                    'uri': test_uri
                })
        
        return tests
    
    def categorize_tests(self, tests):
        """Categorize tests by specification section and type."""
        categories = {
            'initialization': [],
            'state_entry_exit': [],
            'transitions': [],
            'events': [],
            'datamodel': [],
            'history': [],
            'final': [],
            'parallel': [],
            'other': []
        }
        
        for test in tests:
            specnum = test['specnum']
            desc = test['description'].lower()
            
            if 'initial' in desc or specnum == '3.2':
                categories['initialization'].append(test)
            elif 'onentry' in desc or 'onexit' in desc or specnum in ['3.8', '3.9']:
                categories['state_entry_exit'].append(test)
            elif 'transition' in desc and 'history' not in desc:
                categories['transitions'].append(test)
            elif 'event' in desc or 'done.state' in desc:
                categories['events'].append(test)
            elif 'datamodel' in desc or 'data' in desc:
                categories['datamodel'].append(test)
            elif 'history' in desc or specnum == '3.10':
                categories['history'].append(test)
            elif 'final' in desc or specnum == '3.7':
                categories['final'].append(test)
            elif 'parallel' in desc:
                categories['parallel'].append(test)
            else:
                categories['other'].append(test)
        
        return categories
    
    def download_tests(self, categories=None, limit=None):
        """Download W3C SCXML test files."""
        os.makedirs(self.output_dir, exist_ok=True)
        
        # Download manifest
        manifest_path = os.path.join(self.output_dir, "manifest.xml")
        if not self.download_file(self.manifest_url, manifest_path):
            return False
        
        # Parse manifest
        tests = self.parse_manifest(manifest_path)
        categorized = self.categorize_tests(tests)
        
        print(f"\nFound {len(tests)} total tests:")
        for category, test_list in categorized.items():
            print(f"  {category}: {len(test_list)} tests")
        
        # Filter tests if categories specified
        tests_to_download = []
        if categories:
            for category in categories:
                if category in categorized:
                    tests_to_download.extend(categorized[category])
        else:
            tests_to_download = tests
        
        # Limit number of tests if specified
        if limit:
            tests_to_download = tests_to_download[:limit]
        
        print(f"\nDownloading {len(tests_to_download)} tests...")
        
        success_count = 0
        for test in tests_to_download:
            test_url = f"{self.base_url}/{test['uri']}"
            test_dir = os.path.join(self.output_dir, test['id'])
            os.makedirs(test_dir, exist_ok=True)
            
            test_path = os.path.join(test_dir, f"test{test['id']}.txml")
            if self.download_file(test_url, test_path):
                success_count += 1
                
                # Create metadata file
                metadata = {
                    'id': test['id'],
                    'specnum': test['specnum'],
                    'conformance': test['conformance'],
                    'manual': test['manual'],
                    'description': test['description']
                }
                
                metadata_path = os.path.join(test_dir, "metadata.txt")
                with open(metadata_path, 'w') as f:
                    for key, value in metadata.items():
                        f.write(f"{key}: {value}\n")
        
        print(f"\nSuccessfully downloaded {success_count}/{len(tests_to_download)} tests")
        return success_count > 0


def main():
    parser = argparse.ArgumentParser(description='Download W3C SCXML test suite')
    parser.add_argument('--output', '-o', default='w3c-tests', 
                       help='Output directory for tests')
    parser.add_argument('--categories', '-c', nargs='+',
                       choices=['initialization', 'state_entry_exit', 'transitions', 
                               'events', 'datamodel', 'history', 'final', 'parallel', 'other'],
                       help='Test categories to download')
    parser.add_argument('--limit', '-l', type=int,
                       help='Limit number of tests to download')
    parser.add_argument('--list', action='store_true',
                       help='List available test categories and exit')
    
    args = parser.parse_args()
    
    downloader = W3CTestDownloader(args.output)
    
    if args.list:
        # Download manifest and show categories
        os.makedirs(args.output, exist_ok=True)
        manifest_path = os.path.join(args.output, "manifest.xml")
        if downloader.download_file(downloader.manifest_url, manifest_path):
            tests = downloader.parse_manifest(manifest_path)
            categorized = downloader.categorize_tests(tests)
            
            print("Available test categories:")
            for category, test_list in categorized.items():
                print(f"  {category}: {len(test_list)} tests")
        return
    
    success = downloader.download_tests(args.categories, args.limit)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()