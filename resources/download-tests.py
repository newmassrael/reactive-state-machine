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
    def __init__(self, output_dir="."):
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
            
            # Get all test URIs (for variants like test403a.txml, test403b.txml, test403c.txml)
            start_elems = assert_elem.findall('.//start')
            test_uris = []
            for start_elem in start_elems:
                test_uri = start_elem.get('uri')
                if test_uri:
                    test_uris.append(test_uri)
            
            if test_uris:
                tests.append({
                    'id': assert_id,
                    'specnum': specnum,
                    'conformance': conformance,
                    'manual': manual,
                    'description': cdata,
                    'uris': test_uris  # Now supports multiple URIs
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
    
    def find_sub_files(self, txml_content, test_id):
        """Find referenced sub files in TXML content."""
        found_files = set()
        
        # Look for patterns like 'test123sub1.scxml', 'test123sub1.txml', 'file:test123sub1.scxml'
        # Also look for auxiliary files like test446.txt
        patterns = [
            rf'test{test_id}sub\d+\.(?:scxml|txml)',  # test216sub1.scxml
            rf'file:test{test_id}sub\d+\.(?:scxml|txml)',  # file:test216sub1.scxml  
            rf'test{test_id}sub\d+',  # test216sub1 (without extension)
            rf'test{test_id}\.(?:txt|xml|json|yaml)',  # test446.txt, test123.xml, etc.
            rf'file:test{test_id}\.(?:txt|xml|json|yaml)',  # file:test446.txt
        ]
        
        for pattern in patterns:
            matches = re.findall(pattern, txml_content, re.IGNORECASE)
            for match in matches:
                # Clean up the match (remove 'file:' prefix)
                clean_match = match.replace('file:', '')
                
                # If no extension and it's a sub file, try both .txml and .scxml
                if '.' not in clean_match and 'sub' in clean_match:
                    found_files.add(f"{clean_match}.txml")
                    found_files.add(f"{clean_match}.scxml")
                else:
                    found_files.add(clean_match)
        
        # Also look for auxiliary files mentioned in src attributes
        # Pattern for src="file:test446.txt" or src="test446.txt"
        src_pattern = rf'src=[\"\'](?:file:)?(?:[^/]*/)?(test{test_id}(?:sub\d+)?\.(?:txt|xml|json|yaml|csv|dat))[\"\']'
        src_matches = re.findall(src_pattern, txml_content, re.IGNORECASE)
        for match in src_matches:
            found_files.add(match)
            
        # Also look for auxiliary files mentioned in comments or text content
        # Pattern for auxiliary files like "Auxiliary File: test446.txt"
        aux_pattern = rf'(?:Auxiliary\s+File:|auxiliary\s+file:|file:)\s*(test{test_id}(?:sub\d+)?\.(?:txt|xml|json|yaml|csv|dat))'
        aux_matches = re.findall(aux_pattern, txml_content, re.IGNORECASE)
        for match in aux_matches:
            found_files.add(match)
        
        return list(found_files)
    
    def download_sub_files(self, test_id, test_dir):
        """Download sub files for a test if they exist."""
        downloaded_count = 0
        
        # First check if main test file exists to scan for references
        main_test_path = os.path.join(test_dir, f"test{test_id}.txml")
        if not os.path.exists(main_test_path):
            return downloaded_count
            
        try:
            with open(main_test_path, 'r', encoding='utf-8') as f:
                content = f.read()
                
            sub_files = self.find_sub_files(content, test_id)
            
            # Remove duplicates while preserving order
            unique_sub_files = []
            seen = set()
            for sub_file in sub_files:
                if sub_file not in seen:
                    unique_sub_files.append(sub_file)
                    seen.add(sub_file)
            
            for sub_file in unique_sub_files:
                # Extract base name without extension
                base_name = sub_file.replace('.scxml', '').replace('.txml', '')
                
                downloaded = False
                
                # Handle auxiliary files (txt, xml, json, etc.)
                if any(sub_file.endswith(ext) for ext in ['.txt', '.xml', '.json', '.yaml', '.csv', '.dat']):
                    aux_url = f"{self.base_url}/{test_id}/{sub_file}"
                    aux_path = os.path.join(test_dir, sub_file)
                    
                    # Skip if file already exists
                    if os.path.exists(aux_path):
                        print(f"  Auxiliary file already exists: {sub_file}")
                        downloaded = True
                    elif self.download_file(aux_url, aux_path):
                        downloaded_count += 1
                        print(f"  Downloaded auxiliary file: {sub_file}")
                        downloaded = True
                    else:
                        # If download failed, remove the empty file
                        if os.path.exists(aux_path):
                            os.remove(aux_path)
                        print(f"  Failed to download auxiliary file: {sub_file}")
                else:
                    # Handle SCXML/TXML sub files
                    # Try to download as .txml first, then .scxml
                    for ext in ['.txml', '.scxml']:
                        sub_url = f"{self.base_url}/{test_id}/{base_name}{ext}"
                        sub_path = os.path.join(test_dir, f"{base_name}{ext}")
                        
                        # Skip if file already exists
                        if os.path.exists(sub_path):
                            print(f"  Sub file already exists: {base_name}{ext}")
                            downloaded = True
                            break
                        
                        if self.download_file(sub_url, sub_path):
                            downloaded_count += 1
                            print(f"  Downloaded sub file: {base_name}{ext}")
                            downloaded = True
                            break  # Success, don't try other extension
                        else:
                            # If download failed, remove the empty file
                            if os.path.exists(sub_path):
                                os.remove(sub_path)
                    
                    if not downloaded:
                        print(f"  Failed to download sub file: {base_name}")
                            
        except Exception as e:
            print(f"Error processing sub files for test {test_id}: {e}")
            
        return downloaded_count
            
        try:
            with open(main_test_path, 'r', encoding='utf-8') as f:
                content = f.read()
                
            sub_files = self.find_sub_files(content, test_id)
            
            for sub_file in sub_files:
                # Try to download as .txml first, then .scxml
                base_name = sub_file.replace('.scxml', '').replace('.txml', '')
                
                for ext in ['.txml', '.scxml']:
                    sub_url = f"{self.base_url}/{test_id}/{base_name}{ext}"
                    sub_path = os.path.join(test_dir, f"{base_name}{ext}")
                    
                    if self.download_file(sub_url, sub_path):
                        downloaded_count += 1
                        print(f"  Downloaded sub file: {base_name}{ext}")
                        break  # Success, don't try other extension
                    else:
                        # If download failed, remove the empty file
                        if os.path.exists(sub_path):
                            os.remove(sub_path)
                            
        except Exception as e:
            print(f"Error processing sub files for test {test_id}: {e}")
            
        return downloaded_count

    def download_sub_files_for_variant(self, test_id, test_dir, variant_filename):
        """Download sub files for a specific test variant."""
        downloaded_count = 0
        
        # Get the variant identifier (e.g., 'a' from 'test403a.txml')
        variant_base = variant_filename.replace('.txml', '').replace('.scxml', '')
        variant_suffix = variant_base.replace(f'test{test_id}', '')  # Extract 'a', 'b', 'c', etc.
        
        variant_path = os.path.join(test_dir, variant_filename)
        if not os.path.exists(variant_path):
            return downloaded_count
            
        try:
            with open(variant_path, 'r', encoding='utf-8') as f:
                content = f.read()
                
            # Find sub files, but also look for variant-specific sub files
            sub_files = self.find_sub_files(content, test_id)
            
            # Also look for variant-specific sub files (e.g., test403asub1.txml)
            if variant_suffix:
                variant_sub_files = self.find_sub_files(content, f"{test_id}{variant_suffix}")
                sub_files.extend(variant_sub_files)
            
            # Remove duplicates while preserving order
            unique_sub_files = []
            seen = set()
            for sub_file in sub_files:
                if sub_file not in seen:
                    unique_sub_files.append(sub_file)
                    seen.add(sub_file)
            
            for sub_file in unique_sub_files:
                # Extract base name without extension
                base_name = sub_file.replace('.scxml', '').replace('.txml', '')
                
                downloaded = False
                
                # Handle auxiliary files (txt, xml, json, etc.)
                if any(sub_file.endswith(ext) for ext in ['.txt', '.xml', '.json', '.yaml', '.csv', '.dat']):
                    aux_url = f"{self.base_url}/{test_id}/{sub_file}"
                    aux_path = os.path.join(test_dir, sub_file)
                    
                    # Skip if file already exists
                    if os.path.exists(aux_path):
                        print(f"    Auxiliary file already exists: {sub_file}")
                        downloaded = True
                    elif self.download_file(aux_url, aux_path):
                        downloaded_count += 1
                        print(f"    Downloaded auxiliary file: {sub_file}")
                        downloaded = True
                    else:
                        # If download failed, remove the empty file
                        if os.path.exists(aux_path):
                            os.remove(aux_path)
                        print(f"    Failed to download auxiliary file: {sub_file}")
                else:
                    # Handle SCXML/TXML sub files
                    # Try to download as .txml first, then .scxml
                    for ext in ['.txml', '.scxml']:
                        sub_url = f"{self.base_url}/{test_id}/{base_name}{ext}"
                        sub_path = os.path.join(test_dir, f"{base_name}{ext}")
                        
                        # Skip if file already exists
                        if os.path.exists(sub_path):
                            print(f"    Sub file already exists: {base_name}{ext}")
                            downloaded = True
                            break
                        
                        if self.download_file(sub_url, sub_path):
                            downloaded_count += 1
                            print(f"    Downloaded sub file: {base_name}{ext}")
                            downloaded = True
                            break  # Success, don't try other extension
                        else:
                            # If download failed, remove the empty file
                            if os.path.exists(sub_path):
                                os.remove(sub_path)
                    
                    if not downloaded:
                        print(f"    Failed to download sub file: {base_name}")
                            
        except Exception as e:
            print(f"Error processing sub files for variant {variant_filename}: {e}")
            
        return downloaded_count

    def discover_test_variants(self, test_id):
        """Discover all variants of a test (e.g., test403a.txml, test403b.txml, test403c.txml)."""
        variants = []
        
        # First try the standard test file
        standard_url = f"{self.base_url}/{test_id}/test{test_id}.txml"
        try:
            urllib.request.urlopen(standard_url)
            variants.append(f"test{test_id}.txml")
        except:
            pass
        
        # Try alphabetic variants (a, b, c, d, etc.)
        for variant_char in 'abcdefghijklmnopqrstuvwxyz':
            variant_url = f"{self.base_url}/{test_id}/test{test_id}{variant_char}.txml"
            try:
                urllib.request.urlopen(variant_url)
                variants.append(f"test{test_id}{variant_char}.txml")
            except:
                break  # Stop at first missing variant
        
        # Try numeric variants (1, 2, 3, etc.)
        for variant_num in range(1, 10):
            variant_url = f"{self.base_url}/{test_id}/test{test_id}{variant_num}.txml"
            try:
                urllib.request.urlopen(variant_url)
                variants.append(f"test{test_id}{variant_num}.txml")
            except:
                break  # Stop at first missing variant
        
        return variants

    def download_single_test(self, test_id):
        """Download a single test by ID, including all variants."""
        test_dir = os.path.join(self.output_dir, test_id)
        os.makedirs(test_dir, exist_ok=True)
        
        print(f"Discovering test {test_id} variants...")
        variants = self.discover_test_variants(test_id)
        
        if not variants:
            print(f"No test files found for test {test_id}")
            return False
        
        print(f"Found {len(variants)} variant(s): {', '.join(variants)}")
        
        downloaded_count = 0
        total_sub_files = 0
        
        for variant in variants:
            variant_url = f"{self.base_url}/{test_id}/{variant}"
            variant_path = os.path.join(test_dir, variant)
            
            print(f"Downloading {variant}...")
            if self.download_file(variant_url, variant_path):
                downloaded_count += 1
                
                # Download sub files for each variant
                sub_count = self.download_sub_files_for_variant(test_id, test_dir, variant)
                total_sub_files += sub_count
            else:
                print(f"Failed to download {variant}")
        
        if downloaded_count > 0:
            print(f"Successfully downloaded {downloaded_count}/{len(variants)} variant(s) for test {test_id}")
            if total_sub_files > 0:
                print(f"Downloaded {total_sub_files} sub/auxiliary files")
            return True
        else:
            print(f"Failed to download test {test_id}")
            return False

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
        total_sub_files = 0
        
        for test in tests_to_download:
            test_id = test['id']
            test_dir = os.path.join(self.output_dir, test_id)
            os.makedirs(test_dir, exist_ok=True)
            
            # Use URIs from manifest (supports multiple variants)
            test_uris = test.get('uris', [test.get('uri', f"{test_id}/test{test_id}.txml")])
            if isinstance(test_uris, str):
                test_uris = [test_uris]
            
            variant_success_count = 0
            test_sub_files = 0
            
            for test_uri in test_uris:
                # Extract filename from URI (e.g., "403/test403a.txml" -> "test403a.txml")
                filename = test_uri.split('/')[-1]
                
                variant_url = f"{self.base_url}/{test_uri}"
                variant_path = os.path.join(test_dir, filename)
                
                if self.download_file(variant_url, variant_path):
                    variant_success_count += 1
                    
                    # Download sub files for each variant
                    sub_count = self.download_sub_files_for_variant(test_id, test_dir, filename)
                    test_sub_files += sub_count
            
            if variant_success_count > 0:
                success_count += 1
                total_sub_files += test_sub_files
                
                # Create metadata file
                metadata = {
                    'id': test['id'],
                    'specnum': test['specnum'],
                    'conformance': test['conformance'],
                    'manual': test['manual'],
                    'description': test['description'],
                    'variants': len(test_uris)
                }
                
                metadata_path = os.path.join(test_dir, "metadata.txt")
                with open(metadata_path, 'w') as f:
                    for key, value in metadata.items():
                        f.write(f"{key}: {value}\n")
        
        print(f"\nSuccessfully downloaded {success_count}/{len(tests_to_download)} tests")
        print(f"Downloaded {total_sub_files} sub files")
        return success_count > 0


def main():
    parser = argparse.ArgumentParser(description='Download W3C SCXML test suite')
    parser.add_argument('--output', '-o', default='.', 
                       help='Output directory for tests')
    parser.add_argument('--categories', '-c', nargs='+',
                       choices=['initialization', 'state_entry_exit', 'transitions', 
                               'events', 'datamodel', 'history', 'final', 'parallel', 'other'],
                       help='Test categories to download')
    parser.add_argument('--limit', '-l', type=int,
                       help='Limit number of tests to download')
    parser.add_argument('--list', action='store_true',
                       help='List available test categories and exit')
    parser.add_argument('--test-id', '-t', type=str,
                       help='Download specific test by ID (e.g., 216)')
    
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
    
    if args.test_id:
        # Download specific test by ID
        success = downloader.download_single_test(args.test_id)
        sys.exit(0 if success else 1)
    
    success = downloader.download_tests(args.categories, args.limit)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()