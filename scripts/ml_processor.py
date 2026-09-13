#!/usr/bin/env python3
"""
scripts/ml_processor.py
Document Semantic Analysis & Extractive ML Intelligence using Scikit-Learn.
Performs:
1. TF-IDF N-gram Key Term & Concept Extraction
2. Cosine Similarity TextRank Extractive Summarization
3. Document Lexical Statistics & Complexity Scoring
"""

import sys
import os
import json
import re
import argparse
from collections import Counter
import math

def clean_text(text: str) -> str:
    # Replace non-printable characters and normalize whitespace
    text = re.sub(r'\r\n|\r', '\n', text)
    text = re.sub(r'\t', ' ', text)
    return text.strip()

def split_sentences(text: str):
    # Regex splitting on punctuation while avoiding basic abbreviations
    raw_splits = re.split(r'(?<=[.!?])\s+(?=[A-Z0-9])', text)
    sentences = []
    for s in raw_splits:
        s_clean = s.strip()
        # Filter out headers, empty lines, and very short fragments
        if len(s_clean) >= 20 and ' ' in s_clean:
            # Collapse excessive internal whitespace
            s_clean = re.sub(r'\s+', ' ', s_clean)
            sentences.append(s_clean)
    return sentences

def extract_definition_candidates(sentences):
    """Finds sentences that look like definitions (X is ..., X refers to ...)"""
    def_patterns = [
        re.compile(r'^\s*([A-Za-z0-9\s\-]+)\s*[:—–\-]\s*(.+)', re.IGNORECASE),
        re.compile(r'\b([A-Za-z0-9\s\-]{3,35})\s+(?:is defined as|refers to|is a|are)\s+(.+)', re.IGNORECASE)
    ]
    definitions = []
    seen = set()
    for s in sentences:
        for p in def_patterns:
            m = p.search(s)
            if m:
                term = m.group(1).strip()
                desc = m.group(2).strip()
                if 2 <= len(term.split()) <= 4 and term.lower() not in seen:
                    seen.add(term.lower())
                    definitions.append({
                        "term": term.title(),
                        "context": s[:200]
                    })
                    break
        if len(definitions) >= 5:
            break
    return definitions

def process_with_sklearn(text: str, subject: str = "General", level: str = "Undergraduate"):
    from sklearn.feature_extraction.text import TfidfVectorizer
    from sklearn.metrics.pairwise import cosine_similarity
    import numpy as np

    sentences = split_sentences(text)
    if not sentences:
        sentences = [text[:200]] if text else ["Empty document."]

    words = re.findall(r'\b[a-zA-Z]{3,}\b', text.lower())
    total_words = len(words)
    unique_words = len(set(words))
    lexical_diversity = round(unique_words / max(total_words, 1), 3)
    study_minutes = max(1, math.ceil(total_words / 180))

    # Determine complexity
    if lexical_diversity > 0.45 or total_words > 3000:
        complexity = "Advanced Academic"
    elif lexical_diversity > 0.30:
        complexity = "Undergraduate"
    else:
        complexity = "Introductory"

    # TF-IDF for Keywords (unigrams & bigrams)
    min_df = 1
    max_features = min(20, max(5, len(sentences)))
    
    stop_words = 'english'
    vectorizer = TfidfVectorizer(
        ngram_range=(1, 2),
        stop_words=stop_words,
        max_features=max_features,
        token_pattern=r'(?u)\b[a-zA-Z]{3,}\b'
    )

    try:
        tfidf_matrix = vectorizer.fit_transform(sentences)
        feature_names = vectorizer.get_feature_names_out()
        term_scores = np.asarray(tfidf_matrix.sum(axis=0)).flatten()
        
        # Normalize scores to 0-100%
        max_score = np.max(term_scores) if len(term_scores) > 0 and np.max(term_scores) > 0 else 1.0
        keywords = []
        for name, score in sorted(zip(feature_names, term_scores), key=lambda x: x[1], reverse=True)[:8]:
            rel_score = int(round((score / max_score) * 100))
            keywords.append({
                "term": name.title(),
                "relevance_pct": max(15, min(100, rel_score))
            })
    except Exception:
        keywords = [{"term": w.title(), "relevance_pct": 80} for w, _ in Counter(words).most_common(6)]

    # Extractive Summarization via Cosine Similarity (TextRank principle)
    extractive_summary = []
    if len(sentences) >= 3:
        try:
            sim_matrix = cosine_similarity(tfidf_matrix)
            # Sentence centrality = sum of similarities with all other sentences
            centrality = sim_matrix.sum(axis=1)
            # Pick top 3 to 4 sentences
            num_summary = min(4, max(2, len(sentences) // 4))
            ranked_indices = sorted(range(len(centrality)), key=lambda i: centrality[i], reverse=True)[:num_summary]
            # Keep them in original reading order for document coherence
            ranked_indices.sort()
            for idx in ranked_indices:
                extractive_summary.append(sentences[idx])
        except Exception:
            extractive_summary = sentences[:3]
    else:
        extractive_summary = sentences

    definitions = extract_definition_candidates(sentences)

    return {
        "status": "success",
        "engine": "scikit-learn (TfidfVectorizer + Cosine Centrality)",
        "stats": {
            "total_words": total_words,
            "sentence_count": len(sentences),
            "lexical_diversity": lexical_diversity,
            "est_study_minutes": study_minutes,
            "complexity_level": complexity,
            "subject": subject,
            "academic_level": level
        },
        "top_keywords": keywords,
        "extractive_summary": extractive_summary,
        "key_concepts": definitions
    }

def process_fallback(text: str, subject: str = "General", level: str = "Undergraduate"):
    """Pure-Python fallback when scikit-learn is not installed"""
    sentences = split_sentences(text)
    words = re.findall(r'\b[a-zA-Z]{3,}\b', text.lower())
    stopwords = {
        'the', 'and', 'for', 'are', 'with', 'this', 'that', 'from', 'can',
        'have', 'has', 'not', 'will', 'all', 'one', 'two', 'also', 'such',
        'into', 'which', 'when', 'more', 'they', 'their', 'there', 'been'
    }
    filtered_words = [w for w in words if w not in stopwords]
    counts = Counter(filtered_words)
    total_words = len(words)
    unique_words = len(set(words))
    lexical_diversity = round(unique_words / max(total_words, 1), 3)

    most_common = counts.most_common(8)
    max_count = most_common[0][1] if most_common else 1
    keywords = [
        {"term": term.title(), "relevance_pct": int(round((cnt / max_count) * 100))}
        for term, cnt in most_common
    ]

    extractive_summary = sentences[:3] if len(sentences) >= 3 else sentences
    definitions = extract_definition_candidates(sentences)

    return {
        "status": "success",
        "engine": "built-in lexical frequency fallback",
        "stats": {
            "total_words": total_words,
            "sentence_count": len(sentences),
            "lexical_diversity": lexical_diversity,
            "est_study_minutes": max(1, math.ceil(total_words / 180)),
            "complexity_level": "Undergraduate",
            "subject": subject,
            "academic_level": level
        },
        "top_keywords": keywords,
        "extractive_summary": extractive_summary,
        "key_concepts": definitions
    }

def main():
    parser = argparse.ArgumentParser(description="ML Document Processing Engine")
    parser.add_argument("--text-file", help="Path to plain text file to analyze")
    parser.add_argument("--subject", default="Computer Science", help="Subject domain")
    parser.add_argument("--level", default="Undergraduate", help="Academic level")
    args = parser.parse_args()

    try:
        if args.text_file:
            if not os.path.exists(args.text_file):
                print(json.dumps({
                    "status": "error",
                    "message": f"Input file not found: {args.text_file}"
                }))
                sys.exit(0)
            with open(args.text_file, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        else:
            # Read from stdin
            content = sys.stdin.read()

        content = clean_text(content)
        if not content:
            print(json.dumps({
                "status": "error",
                "message": "Empty or unreadable input text"
            }))
            sys.exit(0)

        try:
            result = process_with_sklearn(content, args.subject, args.level)
        except ImportError:
            result = process_fallback(content, args.subject, args.level)

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({
            "status": "error",
            "message": str(e)
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()
